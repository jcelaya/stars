/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
 *
 *  This file is part of STaRS.
 *
 *  STaRS is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  STaRS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with STaRS; if not, see <http://www.gnu.org/licenses/>.
 */

#include "Logger.hpp"
#include "SimpleDispatcher.hpp"
#include "BasicAvailabilityInfo.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"


void SimpleDispatcher::recomputeInfo() {
    LogMsg("Dsp.Simple", DEBUG) << "Recomputing the branch information";
    // Only recalculate info for the father
    std::vector<Link>::iterator child = children.begin();
    for (; child != children.end() && !child->availInfo.get(); child++);
    if (child == children.end()) {
        father.waitingInfo.reset();
        return;
    }
    father.waitingInfo.reset(child->availInfo->clone());
    for (child++; child != children.end(); child++)
        if (child->availInfo.get())
            father.waitingInfo->join(*child->availInfo);
    LogMsg("Dsp.Simple", DEBUG) << "The result is " << *father.waitingInfo;
}


/*
 * A block of info associated with a NodeGroup used in the decision algorithm.
 */
struct SimpleDispatcher::DecisionInfo {
    BasicAvailabilityInfo::MDCluster & cluster;
    unsigned int numBranch;
    double distance;
    uint64_t availability;

    static const uint32_t ALPHA_MEM = 10;
    static const uint32_t ALPHA_DISK = 1;

    DecisionInfo(BasicAvailabilityInfo::MDCluster & c, uint32_t mem, uint32_t disk, unsigned int b, double d)
            : cluster(c), numBranch(b), distance(d), availability((c.minM - mem) * ALPHA_MEM + (c.minD - disk) * ALPHA_DISK) {}

    bool operator<(const DecisionInfo & r) {
        return availability < r.availability || (availability == r.availability && distance < r.distance);
    }
};


void SimpleDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN()) return;
    LogMsg("Dsp.Simple", INFO) << "Received a TaskBagMsg from " << src;
    if (!structureNode.inNetwork()) {
        LogMsg("Dsp.Simple", WARN) << "TaskBagMsg received but not in network";
        return;
    }
    const TaskDescription & req = msg.getMinRequirements();
    unsigned int remainingTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    unsigned int nextTask = msg.getFirstTask();
    LogMsg("Dsp.Simple", INFO) << "Requested allocation of " << remainingTasks << " tasks with requirements:";
    LogMsg("Dsp.Simple", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk();
    LogMsg("Dsp.Simple", INFO) << "Length: " << req.getLength() << "   Deadline: " << req.getDeadline();

    // Distribute it downwards

    // First create a list of node groups which can potentially manage the request
    std::list<DecisionInfo> groups;
    unsigned int numZone = 0;
    for (std::vector<Link>::iterator child = children.begin(); child != children.end(); child++, numZone++) {
        LogMsg("Dsp.Simple", DEBUG) << "Checking zone " << numZone;
        // Ignore the zone that has sent this message, only if it is a StructureNode
        if (child->addr == src && !msg.isFromEN()) {
            LogMsg("Dsp.Simple", DEBUG) << "This zone is the same that sent the message, skiping";
            continue;
        }

        // Ignore zones without information
        if (!child->availInfo.get()) {
            LogMsg("Dsp.Simple", DEBUG) << "This zone has no information, skiping";
            continue;
        }

        // Look for zone information
        double distance = std::numeric_limits<double>::infinity();
        for (StructureNode::zoneConstIterator zone = structureNode.getFirstSubZone();
                zone != structureNode.getLastSubZone(); zone++) {
            if ((*zone)->getLink() == child->addr && (*zone)->getZone().get()) {
                distance = src.distance((*zone)->getZone()->getMinAddress());
                if (src.distance((*zone)->getZone()->getMaxAddress()) < distance)
                    distance = src.distance((*zone)->getZone()->getMaxAddress());
                LogMsg("Dsp.Simple", DEBUG) << "This zone is at distance " << distance;
            }
        }

        std::list<BasicAvailabilityInfo::MDCluster *> nodeGroups;
        child->availInfo->getAvailability(nodeGroups, req);
        LogMsg("Dsp.Simple", DEBUG) << "Obtained " << nodeGroups.size() << " groups with enough availability";
        for (std::list<BasicAvailabilityInfo::MDCluster *>::iterator git = nodeGroups.begin(); git != nodeGroups.end(); git++) {
            LogMsg("Dsp.Simple", DEBUG) << (*git)->value << " nodes with " << (*git)->minM << " memory and " << (*git)->minD << " disk";
            groups.push_back(DecisionInfo(**git, req.getMaxMemory(), req.getMaxDisk(), numZone, distance));
        }
    }
    groups.sort();
    LogMsg("Dsp.Simple", DEBUG) << groups.size() << " groups found";

    // Now divide the request between the zones
    std::vector<unsigned int> numTasks(numZone, 0);
    for (std::list<DecisionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; it++) {
        LogMsg("Dsp.Simple", DEBUG) << "Using group from branch " << it->numBranch << " and " << it->cluster.value << " nodes, availability is " << it->availability;
        if (remainingTasks > it->cluster.value) {
            numTasks[it->numBranch] += it->cluster.value;
            remainingTasks -= it->cluster.value;
            it->cluster.value = 0;
        } else {
            numTasks[it->numBranch] += remainingTasks;
            it->cluster.value -= remainingTasks;
            remainingTasks = 0;
        }
    }
    for (std::vector<Link>::iterator child = children.begin(); child != children.end(); ++child) {
        if (child->availInfo.get())
            child->availInfo->updated();
    }

    // Now create and send the messages
    numZone = 0;
    for (std::vector<Link>::iterator it = children.begin(); it != children.end(); it++, numZone++) {
        if (numTasks[numZone] > 0) {
            LogMsg("Dsp.Simple", INFO) << "Sending " << numTasks[numZone] << " tasks to @" << it->addr;
            // Create the message
            TaskBagMsg * tbm = msg.clone();
            tbm->setForEN(structureNode.isRNChildren());
            tbm->setFromEN(false);
            tbm->setFirstTask(nextTask);
            nextTask += numTasks[numZone];
            tbm->setLastTask(nextTask - 1);
            CommLayer::getInstance().sendMessage(it->addr, tbm);
        }
    }

    // If this branch cannot execute all the tasks, send the request to the father
    if (remainingTasks) {
        LogMsg("Dsp.Simple", INFO) << "There are " << remainingTasks << " remaining tasks";
        if (structureNode.getFather() != CommAddress()) {
            TaskBagMsg * tbm = msg.clone();
            tbm->setFirstTask(nextTask);
            tbm->setLastTask(msg.getLastTask());
            tbm->setFromEN(false);
            if (structureNode.getFather() == src) {
                // Just ignore them
                LogMsg("Dsp.Simple", INFO) << "But came from the father.";
            } else
                CommLayer::getInstance().sendMessage(structureNode.getFather(), tbm);
        } else {
            LogMsg("Dsp.Simple", INFO) << "But we are the root";
        }
    }
}
