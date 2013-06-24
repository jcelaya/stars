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


void MMPDispatcher::recomputeInfo() {
    LogMsg("Dsp.QB", DEBUG) << "Recomputing the branch information";
    // Recalculate info for the father
    if (leftChild.availInfo.get()) {
        father.waitingInfo.reset(leftChild.availInfo->clone());
        if (rightChild.availInfo.get())
            father.waitingInfo->join(*rightChild.availInfo);
        LogMsg("Dsp.QB", DEBUG) << "The result is " << *father.waitingInfo;
    } else if (rightChild.availInfo.get()) {
        father.waitingInfo.reset(rightChild.availInfo->clone());
        LogMsg("Dsp.QB", DEBUG) << "The result is " << *father.waitingInfo;
    } else
        father.waitingInfo.reset();

    if(!branch.isLeftLeaf()) {
        LogMsg("Dsp.QB", DEBUG) << "Recomputing the information from the rest of the tree for left child.";
        // TODO: send full information, based on configuration switch
        if (father.availInfo.get()) {
            Time fatherMaxQueue = father.availInfo->getMaxQueueLength();
            leftChild.waitingInfo.reset(new MMPAvailabilityInformation);
            if (rightChild.availInfo.get()) {
                Time rightMaxQueue = rightChild.availInfo->getMaxQueueLength();
                leftChild.waitingInfo->setMaxQueueLength(fatherMaxQueue > rightMaxQueue ? fatherMaxQueue : rightMaxQueue);
            } else
                leftChild.waitingInfo->setMaxQueueLength(fatherMaxQueue);
        } else if (rightChild.availInfo.get()) {
            leftChild.waitingInfo.reset(new MMPAvailabilityInformation);
            leftChild.waitingInfo->setMaxQueueLength(rightChild.availInfo->getMaxQueueLength());
        } else
            leftChild.waitingInfo.reset();
    }

    if(!branch.isRightLeaf()) {
        LogMsg("Dsp.QB", DEBUG) << "Recomputing the information from the rest of the tree for right child.";
        // TODO: send full information, based on configuration switch
        if (father.availInfo.get()) {
            Time fatherMaxQueue = father.availInfo->getMaxQueueLength();
            rightChild.waitingInfo.reset(new MMPAvailabilityInformation);
            if (leftChild.availInfo.get()) {
                Time leftMaxQueue = leftChild.availInfo->getMaxQueueLength();
                rightChild.waitingInfo->setMaxQueueLength(fatherMaxQueue > leftMaxQueue ? fatherMaxQueue : leftMaxQueue);
            } else
                rightChild.waitingInfo->setMaxQueueLength(fatherMaxQueue);
        } else if (leftChild.availInfo.get()) {
            rightChild.waitingInfo.reset(new MMPAvailabilityInformation);
            rightChild.waitingInfo->setMaxQueueLength(leftChild.availInfo->getMaxQueueLength());
        } else
            rightChild.waitingInfo.reset();
    }
}


/*
 * A block of info associated with a NodeGroup used in the decission algorithm.
 */
struct MMPDispatcher::DecissionInfo {
    MMPAvailabilityInformation::MDPTCluster * cluster;
    bool leftBranch;
    double distance;
    double availability;
    unsigned int numTasks;

    static const uint32_t ALPHA_MEM = 10;
    static const uint32_t ALPHA_DISK = 1;
    static const uint32_t ALPHA_TIME = 100;

    DecissionInfo(MMPAvailabilityInformation::MDPTCluster * c, const TaskDescription & req, bool b, double d)
            : cluster(c), leftBranch(b), distance(d) {
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
    if (msg.isForEN()) return;
    LogMsg("Dsp.QB", INFO) << "Received a TaskBagMsg from " << src;
    if (!branch.inNetwork()) {
        LogMsg("Dsp.QB", WARN) << "TaskBagMsg received but not in network";
        return;
    }
    boost::shared_ptr<MMPAvailabilityInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
    if (!zoneInfo.get()) {
        LogMsg("Dsp.QB", WARN) << "TaskBagMsg received but no information!";
        return;
    }

    TaskDescription req = msg.getMinRequirements();
    unsigned int remainingTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    unsigned int nextTask = msg.getFirstTask();
    LogMsg("Dsp.QB", INFO) << "Requested allocation of " << remainingTasks << " tasks with requirements:";
    LogMsg("Dsp.QB", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk();
    LogMsg("Dsp.QB", INFO) << "Length: " << req.getLength();

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
        LogMsg("Dsp.QB", DEBUG) << "Before the minimum queue (" << fatherInfo << ") there is space for " << tasks << " tasks";

        if (tasks < remainingTasks && (src != father.addr || msg.isFromEN())) {
            // Send it up if there are not enough nodes, we are not the root and the sender is not our father or it is a child with the same address
            TaskBagMsg * tbm = msg.clone();
            tbm->setFromEN(false);
            CommLayer::getInstance().sendMessage(father.addr, tbm);
            LogMsg("Dsp.QB", INFO) << "Not enough nodes, send to the father";
            return;
        }
    }

    // If there are enough tasks, distribute it downwards
    Time balancedQueue = zoneInfo->getAvailability(nodeGroups, remainingTasks, req);
    if (balancedQueue == Time()) {
        LogMsg("Dsp.QB", WARN) << "No node fulfills requirements, dropping!";
        return;
    }
    req.setDeadline(balancedQueue);
    father.waitingInfo.reset(zoneInfo->clone());
    father.waitingInfo->updateAvailability(req);
    LogMsg("Dsp.QB", DEBUG) << "The calculated queue length is " << balancedQueue;

    // Calculate distances
    const CommAddress & requester = msg.getRequester();
    double leftDistance = branch.getLeftDistance(requester),
            rightDistance = branch.getRightDistance(requester);

    // First create a list of node groups which can potentially manage the request
    std::list<DecissionInfo> groups;
    if (leftChild.availInfo.get()) {
        nodeGroups.clear();
        leftChild.availInfo->getAvailability(nodeGroups, req);
        LogMsg("Dsp.QB", DEBUG) << "Obtained " << nodeGroups.size() << " groups with enough availability from left child.";
        for (std::list<MMPAvailabilityInformation::MDPTCluster *>::iterator git = nodeGroups.begin(); git != nodeGroups.end(); git++) {
            LogMsg("Dsp.QB", DEBUG) << (*git)->getValue() << " tasks of size availability " << req.getLength();
            groups.push_back(DecissionInfo(*git, req, true, leftDistance));
        }
    }
    if (rightChild.availInfo.get()) {
        nodeGroups.clear();
        rightChild.availInfo->getAvailability(nodeGroups, req);
        LogMsg("Dsp.QB", DEBUG) << "Obtained " << nodeGroups.size() << " groups with enough availability from right child.";
        for (std::list<MMPAvailabilityInformation::MDPTCluster *>::iterator git = nodeGroups.begin(); git != nodeGroups.end(); git++) {
            LogMsg("Dsp.QB", DEBUG) << (*git)->getValue() << " tasks of size availability " << req.getLength();
            groups.push_back(DecissionInfo(*git, req, true, leftDistance));
        }
    }
    LogMsg("Dsp.QB", DEBUG) << groups.size() << " groups found";
    groups.sort();

    // Now divide the request between the zones
    unsigned int leftTasks = 0, rightTasks = 0;
    for (std::list<DecissionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; it++) {
        LogMsg("Dsp.QB", DEBUG) << "Using group from " << (it->leftBranch ? "left" : "right") << " branch and " << it->numTasks << " tasks";
        unsigned int tasksInGroup = it->numTasks;
        if (remainingTasks > tasksInGroup) {
            (it->leftBranch ? leftTasks : rightTasks) += tasksInGroup;
            remainingTasks -= tasksInGroup;
        } else {
            (it->leftBranch ? leftTasks : rightTasks) += remainingTasks;
            remainingTasks = 0;
        }
        it->cluster->updateMaximumQueue(balancedQueue);
    }

    if (leftTasks > 0)
        leftChild.availInfo->updateMaxT(balancedQueue);
    if (rightTasks > 0)
        rightChild.availInfo->updateMaxT(balancedQueue);

    // Now create and send the messages, do not send up remaining tasks
    sendTasks(msg, leftTasks, rightTasks, true);
}
