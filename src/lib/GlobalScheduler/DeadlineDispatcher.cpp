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
#include "DeadlineDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using namespace std;


const Duration DeadlineDispatcher::REQUEST_CACHE_TIME(10.0);
const unsigned int DeadlineDispatcher::REQUEST_CACHE_SIZE = 100;


void DeadlineDispatcher::recomputeInfo() {
    LogMsg("Dsp.Dl", DEBUG) << "Recomputing the branch information";
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
    LogMsg("Dsp.Dl", DEBUG) << "The result is " << *father.waitingInfo;
}


/*
 * A block of info associated with a NodeGroup used in the decission algorithm.
 */
struct DeadlineDispatcher::DecissionInfo {
    TimeConstraintInfo::AssignmentInfo ai;
    unsigned int numBranch;
    double distance;
    uint64_t availability;

    static const uint32_t ALPHA_MEM = 10;
    static const uint32_t ALPHA_DISK = 1;
    static const uint32_t ALPHA_COMP = 100;

    DecissionInfo(const TimeConstraintInfo::AssignmentInfo & c, unsigned int b, double d)
            : ai(c), numBranch(b), distance(d) {
        availability = ALPHA_MEM * ai.remngMem + ALPHA_DISK * ai.remngDisk + ALPHA_COMP * ai.remngAvail;
    }

    bool operator<(const DecissionInfo & r) {
        return availability < r.availability || (availability == r.availability && distance < r.distance);
    }
};


void DeadlineDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN()) return;
    LogMsg("Dsp.Dl", INFO) << "Received a TaskBagMsg from " << src;
    if (!structureNode.inNetwork()) {
        LogMsg("Dsp.Dl", WARN) << "TaskBagMsg received but not in network";
        return;
    }

    // Check if we already routed this request recently
    Time now = Time::getCurrentTime();
    {
        list<RecentRequest>::iterator it = recentRequests.begin();
        while (it != recentRequests.end() && now - it->when > REQUEST_CACHE_TIME)
            it = recentRequests.erase(it);
        while (it != recentRequests.end() && !(it->requestId == msg.getRequestId() && it->requester == msg.getRequester()))
            it++;
        if (it != recentRequests.end()) {
            recentRequests.push_back(RecentRequest(msg.getRequester(), msg.getRequestId(), now));
            recentRequests.erase(it);
            if (structureNode.getFather() != CommAddress()) {
                CommLayer::getInstance().sendMessage(structureNode.getFather(), msg.clone());
            }
            return;
        }
    }
    // The request wasn't in the cache
    recentRequests.push_back(RecentRequest(msg.getRequester(), msg.getRequestId(), now));
    if (recentRequests.size() > REQUEST_CACHE_SIZE) recentRequests.pop_front();

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int remainingTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    unsigned int nextTask = msg.getFirstTask();
    LogMsg("Dsp.Dl", INFO) << "Requested allocation of " << remainingTasks << " tasks with requirements:";
    LogMsg("Dsp.Dl", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk();
    LogMsg("Dsp.Dl", INFO) << "Length: " << req.getLength() << "   Deadline: " << req.getDeadline();

    // Distribute it downwards

    // First create a list of node groups which can potentially manage the request
    list<DecissionInfo> groups;
    for (unsigned int numZone = 0; numZone < children.size(); numZone++) {
        Link & child = children[numZone];
        LogMsg("Dsp.Dl", DEBUG) << "Checking zone " << numZone;
        // Ignore the zone that has sent this message, only if it is a StructureNode
        if (child.addr == src && !msg.isFromEN()) {
            LogMsg("Dsp.Dl", DEBUG) << "This zone is the same that sent the message, skiping";
            continue;
        }

        // Ignore zones without information
        if (!child.availInfo.get()) {
            LogMsg("Dsp.Dl", DEBUG) << "This zone has no information, skiping";
            continue;
        }

        // Look for zone information
        const CommAddress & requester = msg.getRequester();
        double distance = numeric_limits<double>::infinity();
        for (StructureNode::zoneConstIterator zone = structureNode.getFirstSubZone();
                zone != structureNode.getLastSubZone(); zone++) {
            if ((*zone)->getLink() == child.addr && (*zone)->getZone().get()) {
                distance = requester.distance((*zone)->getZone()->getMinAddress());
                if (requester.distance((*zone)->getZone()->getMaxAddress()) < distance)
                    distance = requester.distance((*zone)->getZone()->getMaxAddress());
                LogMsg("Dsp.Dl", DEBUG) << "This zone is at distance " << distance;
            }
        }

        list<TimeConstraintInfo::AssignmentInfo> ai;
        child.availInfo->getAvailability(ai, req);
        LogMsg("Dsp.Dl", DEBUG) << "Obtained " << ai.size() << " groups with enough availability";
        for (list<TimeConstraintInfo::AssignmentInfo>::iterator git = ai.begin();
                git != ai.end(); git++) {
            LogMsg("Dsp.Dl", DEBUG) << git->numTasks << " tasks with remaining availability " << git->remngAvail;
            groups.push_back(DecissionInfo(*git, numZone, distance));
        }
    }
    groups.sort();
    LogMsg("Dsp.Dl", DEBUG) << groups.size() << " groups found";

    // Now divide the request between the zones
    vector<list<TimeConstraintInfo::AssignmentInfo> > finalAssignment(children.size());
    vector<unsigned int> numTasks(children.size(), 0);
    for (list<DecissionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; it++) {
        LogMsg("Dsp.Dl", DEBUG) << "Using group from branch " << it->numBranch << " and " << it->ai.numTasks << " tasks";
        unsigned int tasksInGroup = it->ai.numTasks;
        if (remainingTasks > tasksInGroup) {
            numTasks[it->numBranch] += tasksInGroup;
            remainingTasks -= tasksInGroup;
            finalAssignment[it->numBranch].push_back(it->ai);
        } else {
            numTasks[it->numBranch] += remainingTasks;
            finalAssignment[it->numBranch].push_back(it->ai);
            finalAssignment[it->numBranch].back().numTasks = remainingTasks;
            remainingTasks = 0;
        }
    }

    // Update the availability information
    for (unsigned int numZone = 0; numZone < children.size(); numZone++) {
        if (children[numZone].availInfo.get())
            children[numZone].availInfo->update(finalAssignment[numZone], req);
    }
    // Send the result to the father
    recomputeInfo();
    notify();

    // Now create and send the messages
    for (unsigned int numZone = 0; numZone < children.size(); numZone++) {
        Link & child = children[numZone];
        if (numTasks[numZone] > 0) {
            LogMsg("Dsp.Dl", INFO) << "Sending " << numTasks[numZone] << " tasks to @" << child.addr;
            // Create the message
            TaskBagMsg * tbm = msg.clone();
            tbm->setForEN(structureNode.isRNChildren());
            tbm->setFromEN(false);
            tbm->setFirstTask(nextTask);
            nextTask += numTasks[numZone];
            tbm->setLastTask(nextTask - 1);
            CommLayer::getInstance().sendMessage(child.addr, tbm);
        }
    }

    // If this branch cannot execute all the tasks, send the request to the father
    if (remainingTasks) {
        LogMsg("Dsp.Dl", INFO) << "There are " << remainingTasks << " remaining tasks";
        if (structureNode.getFather() != CommAddress()) {
            TaskBagMsg * tbm = msg.clone();
            tbm->setFirstTask(nextTask);
            tbm->setLastTask(msg.getLastTask());
            tbm->setFromEN(false);
            CommLayer::getInstance().sendMessage(structureNode.getFather(), tbm);
        } else {
            LogMsg("Dsp.Dl", INFO) << "But we are the root";
        }
    }
}
