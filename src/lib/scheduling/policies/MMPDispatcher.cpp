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
#include "MMPDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using boost::shared_ptr;


double MMPDispatcher::beta = 0.5;


boost::shared_ptr<MMPAvailabilityInformation> MMPDispatcher::getChildInfo(int c) {
    const Link & other = child[c^1];
    boost::shared_ptr<MMPAvailabilityInformation> result;
    // TODO: send full information, based on configuration switch
    if (father.availInfo.get()) {
        Time fatherMaxQueue = father.availInfo->getMaxQueueLength();
        result.reset(new MMPAvailabilityInformation);
        if (other.availInfo.get()) {
            Time otherMaxQueue = other.availInfo->getMaxQueueLength();
            result->setMaxQueueLength(fatherMaxQueue > otherMaxQueue ? fatherMaxQueue : otherMaxQueue);
        } else
            result->setMaxQueueLength(fatherMaxQueue);
    } else if (other.availInfo.get()) {
        result.reset(new MMPAvailabilityInformation);
        result->setMaxQueueLength(other.availInfo->getMaxQueueLength());
    }
    return result;
}


void MMPDispatcher::recomputeInfo() {
    LogMsg("Dsp.MMP", DEBUG) << "Recomputing the branch information";
    // Recalculate info for the father
    recomputeFatherInfo();

    for (int c : {0, 1}) {
        if(!branch.isLeaf(c)) {
            child[c].waitingInfo = getChildInfo(c);
        }
    }
}


/*
 * A block of info associated with a NodeGroup used in the decission algorithm.
 */
struct MMPDispatcher::DecissionInfo {
    MMPAvailabilityInformation::MDPTCluster * cluster;
    int branch;
    double distance;
    double availability;
    unsigned int numTasks;

    static const uint32_t ALPHA_MEM = 10;
    static const uint32_t ALPHA_DISK = 1;
    static const uint32_t ALPHA_TIME = 100;

    DecissionInfo(MMPAvailabilityInformation::MDPTCluster * c, const TaskDescription & req, int b, double d)
            : cluster(c), branch(b), distance(d) {
        double oneTaskTime = req.getLength() / (double)c->getMinimumPower();
        availability = ALPHA_MEM * c->getLostMemory(req)
                + ALPHA_DISK * c->getLostDisk(req)
                + ALPHA_TIME / ((req.getDeadline() - c->getMaximumQueue()).seconds() + oneTaskTime);
        numTasks = c->getValue() * ((req.getDeadline() - c->getMaximumQueue()).seconds() / oneTaskTime);
    }

    bool operator<(const DecissionInfo & r) {
        return availability < r.availability || (availability == r.availability && distance < r.distance);
    }
};


void MMPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN() || !checkState()) return;
    LogMsg("Dsp.MMP", INFO) << "Received a TaskBagMsg from " << src;
    boost::shared_ptr<MMPAvailabilityInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;

    TaskDescription req = msg.getMinRequirements();
    unsigned int remainingTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    unsigned int nextTask = msg.getFirstTask();
    LogMsg("Dsp.MMP", INFO) << "Requested allocation of " << remainingTasks << " tasks with requirements:";
    LogMsg("Dsp.MMP", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk();
    LogMsg("Dsp.MMP", INFO) << "Length: " << req.getLength();

    std::list<MMPAvailabilityInformation::MDPTCluster *> nodeGroups;
    if (father.addr != CommAddress()) {
        // Count number of tasks before the minimum length in the rest of the tree
        Time now = Time::getCurrentTime();
        //Time fatherInfo = father.availInfo.get() && (father.availInfo->getMinQueueLength() > now) ? father.availInfo->getMinQueueLength() : now;
        Time fatherInfo = father.availInfo.get() && (father.availInfo->getMaxQueueLength() > now) ? father.availInfo->getMaxQueueLength() : now;
        // Adjust the minimum length by beta
        //Duration oneTaskSlowTime(req.getLength() / zoneInfo->getMinPower());
        //fatherInfo += oneTaskSlowTime * beta;
        fatherInfo = now + Duration(fatherInfo - now) * beta;
        req.setDeadline(fatherInfo);
        unsigned int tasks = zoneInfo->getAvailability(nodeGroups, req);
        LogMsg("Dsp.MMP", DEBUG) << "Before the minimum queue (" << fatherInfo << ") there is space for " << tasks << " tasks";

        if (tasks < remainingTasks && (src != father.addr || msg.isFromEN())) {
            // Send it up if there are not enough nodes, we are not the root and the sender is not our father or it is a child with the same address
            TaskBagMsg * tbm = msg.clone();
            tbm->setFromEN(false);
            CommLayer::getInstance().sendMessage(father.addr, tbm);
            LogMsg("Dsp.MMP", INFO) << "Not enough nodes, send to the father";
            return;
        }
    }

    // If there are enough tasks, distribute it downwards
    Time balancedQueue = zoneInfo->getAvailability(nodeGroups, remainingTasks, req);
    if (balancedQueue == Time()) {
        LogMsg("Dsp.MMP", WARN) << "No node fulfills requirements, dropping!";
        return;
    }
    req.setDeadline(balancedQueue);
    father.waitingInfo.reset(zoneInfo->clone());
    father.waitingInfo->updateAvailability(req);
    LogMsg("Dsp.MMP", DEBUG) << "The calculated queue length is " << balancedQueue;

    // Calculate distances
    const CommAddress & requester = msg.getRequester();

    // First create a list of node groups which can potentially manage the request
    std::list<DecissionInfo> groups;
    for (int c : {0, 1}) {
        if (child[c].availInfo.get()) {
            nodeGroups.clear();
            child[c].availInfo->getAvailability(nodeGroups, req);
            LogMsg("Dsp.MMP", DEBUG) << "Obtained " << nodeGroups.size() << " groups with enough availability from " << c << " child.";
            for (std::list<MMPAvailabilityInformation::MDPTCluster *>::iterator git = nodeGroups.begin(); git != nodeGroups.end(); git++) {
                LogMsg("Dsp.MMP", DEBUG) << (*git)->getValue() << " tasks of size availability " << req.getLength();
                groups.push_back(DecissionInfo(*git, req, c, branch.getChildDistance(c, requester)));
            }
        }
    }
    LogMsg("Dsp.MMP", DEBUG) << groups.size() << " groups found";
    groups.sort();

    // Now divide the request between the zones
    unsigned int numTasks[2] = { 0, 0 };
    for (std::list<DecissionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; it++) {
        LogMsg("Dsp.MMP", DEBUG) << "Using group from " << (it->branch == 0 ? "left" : "right") << " branch and " << it->numTasks << " tasks";
        unsigned int tasksInGroup = it->numTasks;
        if (remainingTasks > tasksInGroup) {
            numTasks[it->branch] += tasksInGroup;
            remainingTasks -= tasksInGroup;
        } else {
            numTasks[it->branch] += remainingTasks;
            remainingTasks = 0;
        }
        it->cluster->updateMaximumQueue(balancedQueue);
    }

    for (int c : {0, 1})
        if (numTasks[c] > 0)
            child[c].availInfo->updateMaxT(balancedQueue);

    // Now create and send the messages, do not send up remaining tasks
    sendTasks(msg, numTasks, true);
}
