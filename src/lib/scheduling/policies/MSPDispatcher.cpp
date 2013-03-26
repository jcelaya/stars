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
#include "MSPDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using std::vector;


double MSPDispatcher::beta = 2.0;
//int MSPDispatcher::estimations = 0;


void MSPDispatcher::recomputeInfo() {
    LogMsg("Dsp.MS", DEBUG) << "Recomputing the branch information";
    // Recalculate info for the father
    if (leftChild.availInfo.get()) {
        father.waitingInfo.reset(leftChild.availInfo->clone());
        if (rightChild.availInfo.get())
            father.waitingInfo->join(*rightChild.availInfo);
        LogMsg("Dsp.MS", DEBUG) << "The result is " << *father.waitingInfo;
    } else if (rightChild.availInfo.get()) {
        father.waitingInfo.reset(rightChild.availInfo->clone());
        LogMsg("Dsp.MS", DEBUG) << "The result is " << *father.waitingInfo;
    } else
        father.waitingInfo.reset();

    if(!branch.isLeftLeaf()) {
        LogMsg("Dsp.MS", DEBUG) << "Recomputing the information from the rest of the tree for left child.";
        // TODO: send full information, based on configuration switch
        if (father.availInfo.get()) {
            double fatherMaxSlowness = father.availInfo->getMaximumSlowness();
            leftChild.waitingInfo.reset(new MSPAvailabilityInformation);
            if (rightChild.availInfo.get()) {
                double rightMaxSlowness = rightChild.availInfo->getMaximumSlowness();
                leftChild.waitingInfo->setMaximumSlowness(fatherMaxSlowness > rightMaxSlowness ? fatherMaxSlowness : rightMaxSlowness);
            } else
                leftChild.waitingInfo->setMaximumSlowness(fatherMaxSlowness);
        } else if (rightChild.availInfo.get()) {
            leftChild.waitingInfo.reset(new MSPAvailabilityInformation);
            leftChild.waitingInfo->setMaximumSlowness(rightChild.availInfo->getMaximumSlowness());
        } else
            leftChild.waitingInfo.reset();
    }

    if(!branch.isRightLeaf()) {
        LogMsg("Dsp.MS", DEBUG) << "Recomputing the information from the rest of the tree for right child.";
        // TODO: send full information, based on configuration switch
        if (father.availInfo.get()) {
            double fatherMaxSlowness = father.availInfo->getMaximumSlowness();
            rightChild.waitingInfo.reset(new MSPAvailabilityInformation);
            if (leftChild.availInfo.get()) {
                double leftMaxSlowness = leftChild.availInfo->getMaximumSlowness();
                rightChild.waitingInfo->setMaximumSlowness(fatherMaxSlowness > leftMaxSlowness ? fatherMaxSlowness : leftMaxSlowness);
            } else
                rightChild.waitingInfo->setMaximumSlowness(fatherMaxSlowness);
        } else if (leftChild.availInfo.get()) {
            rightChild.waitingInfo.reset(new MSPAvailabilityInformation);
            rightChild.waitingInfo->setMaximumSlowness(leftChild.availInfo->getMaximumSlowness());
        } else
            rightChild.waitingInfo.reset();
    }
}


struct SlownessTasks {
    double slowness;
    int numTasks;
    unsigned int i;
    void set(double s, int n, int index) { slowness = s; numTasks = n; i = index; }
    bool operator<(const SlownessTasks & o) const { return slowness < o.slowness || (slowness == o.slowness && numTasks < o.numTasks); }
};


void MSPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN()) return;

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int numTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    uint64_t a = req.getLength();

    // TODO: Check that we are not in a change
    if (!msg.isFromEN() && src == father.addr) {
        LogMsg("Dsp.MS", INFO) << "Received a TaskBagMsg from " << src << " (father)";
    } else {
        LogMsg("Dsp.MS", INFO) << "Received a TaskBagMsg from " << src << " (" << (src == leftChild.addr ? "left child)" : "right child)");
    }
    if (!branch.inNetwork()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but not in network";
        return;
    }
    boost::shared_ptr<MSPAvailabilityInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
    if (!zoneInfo.get()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but no information!";
        return;
    }

    LogMsg("Dsp.MS", INFO) << "Requested allocation of request " << msg.getRequestId() << " with " << numTasks << " tasks with requirements:";
    LogMsg("Dsp.MS", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk() << "   Length: " << a;

    // If it is comming from the father, update its notified information
//	if (!msg.isFromEN() && structureNode.getFather() == src && father.availInfo.get()) {
//		father.availInfo->update(numTasks, req.getLength());
//	}

    unsigned int rfStart;
    double minSlowness = INFINITY;
    Time now = Time::getCurrentTime();
    // Get the slowness functions of each branch that fulfills memory and disk requirements
    std::vector<std::pair<MSPAvailabilityInformation::LAFunction *, unsigned int> > functions;
    if (leftChild.availInfo.get()) {
        LogMsg("Dsp.MS", DEBUG) << "Getting functions of left children (" << leftChild.addr << "): " << *leftChild.availInfo;
        leftChild.availInfo->updateRkReference(now);
        leftChild.availInfo->getFunctions(req, functions);
    }
    rfStart = functions.size();
    if (rightChild.availInfo.get()) {
        LogMsg("Dsp.MS", DEBUG) << "Getting functions of left children (" << rightChild.addr << "): " << *rightChild.availInfo;
        rightChild.availInfo->updateRkReference(now);
        rightChild.availInfo->getFunctions(req, functions);
    }

    unsigned int totalTasks = 0;
    // Number of tasks sent to each node
    vector<int> tpn(functions.size(), 0);
    // Heap of slowness values
    vector<std::pair<double, size_t> > slownessHeap;

    if (!functions.empty()) {
        bool tryOneMoreTask = true;
        for (int currentTpn = 1; tryOneMoreTask; ++currentTpn) {
            // Try with one more task per node
            tryOneMoreTask = false;
            for (size_t f = 0; f < functions.size(); ++f) {
                // With those functions that got an additional task per node in the last iteration,
                if (tpn[f] == currentTpn - 1) {
                    // calculate the slowness with one task more
                    double slowness = currentTpn == 1 ?
                            functions[f].first->getSlowness(a)
                            : functions[f].first->estimateSlowness(a, currentTpn);
                    // Check that it is not under the minimum of its branch
                    double branchSlowness = (f < rfStart ? leftChild.availInfo : rightChild.availInfo)->getMinimumSlowness();
                    if (slowness < branchSlowness) {
                        slowness = branchSlowness;
                    }
                    // If the slowness is less than the maximum to date, or there aren't enough tasks yet, insert it in the heap
                    if (totalTasks < numTasks || slowness < slownessHeap.front().first) {
                        slownessHeap.push_back(std::make_pair(slowness, f));
                        std::push_heap(slownessHeap.begin(), slownessHeap.end());
                        // Add one task per node to that function
                        ++tpn[f];
                        totalTasks += functions[f].second;
                        // Remove the highest slowness values as long as there are enough tasks
                        while (totalTasks - functions[slownessHeap.front().second].second >= numTasks) {
                            totalTasks -= functions[slownessHeap.front().second].second;
                            --tpn[slownessHeap.front().second];
                            std::pop_heap(slownessHeap.begin(), slownessHeap.end());
                            slownessHeap.pop_back();
                        }
                        // As long as one function gets another task, we continue trying
                        tryOneMoreTask = true;
                    }
                }
            }
        }
        minSlowness = slownessHeap.front().first;
    }

    LogMsg("Dsp.MS", INFO) << "Result minimum slowness is " << minSlowness;

//    // DEBUG
//    // If the message comes from the father, compare slowness
//    if (!msg.isFromEN() && father.addr != CommAddress() && father.addr == src) {
//        ++estimations;
//        if (minSlowness > msg.slowness) {
//            uint32_t seq = father.notifiedInfo.get() ? father.notifiedInfo->getSeq() : 0;
//            LogMsg("Dsp.MS", WARN) << msg << ", " << minSlowness << " with " << seq << ", " << msg.slowness << " with " << msg.seq << " by source.";
//        }
//    }

    // if we are not the root and the message does not come from the father
    if (father.addr != CommAddress() && (msg.isFromEN() || father.addr != src)) {
        // Compare the slowness reached by the new application with the one in the rest of the tree,
        double slownessLimit = zoneInfo->getMaximumSlowness();
        LogMsg("Dsp.MS", DEBUG) << "The maximum slowness in this branch is " << slownessLimit;
        if (father.availInfo.get()) {
            slownessLimit = father.availInfo->getMaximumSlowness();
            LogMsg("Dsp.MS", DEBUG) << "The maximum slowness in the rest of the tree is " << slownessLimit;
        }
        LogMsg("Dsp.MS", DEBUG) << "The slowest machine in this branch would provide a slowness of " << zoneInfo->getSlowestMachine();
        if (zoneInfo->getSlowestMachine() > slownessLimit)
            slownessLimit = zoneInfo->getSlowestMachine();
        slownessLimit *= beta;
        if (minSlowness > slownessLimit) {
            LogMsg("Dsp.MS", INFO) << "Not enough information to route this request, sending to the father.";
            CommLayer::getInstance().sendMessage(father.addr, msg.clone());
            return;
        }
    }

    // Count tasks per branch and update minimum branch slowness
    unsigned int leftTasks = 0, rightTasks = 0;
    double leftSlowness = leftChild.availInfo.get() ? leftChild.availInfo->getMinimumSlowness() : 0,
            rightSlowness = rightChild.availInfo.get() ? rightChild.availInfo->getMinimumSlowness() : 0;
    for (size_t i = 0; i < rfStart; ++i) {
        if (tpn[i]) {
            double slowness = tpn[i] == 1 ?
                    functions[i].first->getSlowness(a)
                    : functions[i].first->estimateSlowness(a, tpn[i]);
            if (leftSlowness < slowness) {
                leftSlowness = slowness;
            }
            unsigned int tasksToCluster = tpn[i] * functions[i].second;
            if (i == slownessHeap.front().second) {
                tasksToCluster -= totalTasks - numTasks;
            }
            leftTasks += tasksToCluster;
            functions[i].first->update(a, tpn[i]);
        }
    }

    for (size_t i = rfStart; i < tpn.size(); ++i) {
        if (tpn[i]) {
            double slowness = tpn[i] == 1 ?
                    functions[i].first->getSlowness(a)
                    : functions[i].first->estimateSlowness(a, tpn[i]);
            if (rightSlowness < slowness) {
                rightSlowness = slowness;
            }
            unsigned int tasksToCluster = tpn[i] * functions[i].second;
            if (i == slownessHeap.front().second)
                tasksToCluster -= totalTasks - numTasks;
            rightTasks += tasksToCluster;
            functions[i].first->update(a, tpn[i]);
        }
    }

    if (leftChild.availInfo.get()) {
        leftChild.availInfo->setMinimumSlowness(leftSlowness);
        if (leftChild.availInfo->getMaximumSlowness() < leftSlowness) {
            leftChild.availInfo->setMaximumSlowness(leftSlowness);
        }
    }
    if (rightChild.availInfo.get()) {
        rightChild.availInfo->setMinimumSlowness(rightSlowness);
        if (rightChild.availInfo->getMaximumSlowness() < rightSlowness) {
            rightChild.availInfo->setMaximumSlowness(rightSlowness);
        }
    }

    LogMsg("Dsp.MS", DEBUG) << "Sending " << leftTasks << " tasks to left child (" << leftChild.addr << ")"
            " and " << rightTasks << " tasks to right child (" << rightChild.addr << ")";

    // We are going down!
    // Each branch is sent its accounted number of tasks
    sendTasks(msg, leftTasks, rightTasks, false);

//    // DEBUG
//    // Now create and send the messages
//    unsigned int nextTask = msg.getFirstTask();
//    if (leftTasks > 0) {
//        LogMsg("Dsp", INFO) << "Sending " << leftTasks << " tasks to the left child";
//        // Create the message
//        TaskBagMsg * tbm = msg.clone();
//        tbm->setForEN(branch.isLeftLeaf());
//        tbm->setFromEN(false);
//        tbm->setFirstTask(nextTask);
//        nextTask += leftTasks;
//        tbm->setLastTask(nextTask - 1);
//        tbm->slowness = leftSlowness;
//        tbm->seq = leftChild.availInfo->getSeq();
//        CommLayer::getInstance().sendMessage(leftChild.addr, tbm);
//    }
//
//    if (rightTasks > 0) {
//        LogMsg("Dsp", INFO) << "Sending " << rightTasks << " tasks to the right child";
//        // Create the message
//        TaskBagMsg * tbm = msg.clone();
//        tbm->setForEN(branch.isRightLeaf());
//        tbm->setFromEN(false);
//        tbm->setFirstTask(nextTask);
//        nextTask += rightTasks;
//        tbm->setLastTask(nextTask - 1);
//        tbm->slowness = rightSlowness;
//        tbm->seq = rightChild.availInfo->getSeq();
//        CommLayer::getInstance().sendMessage(rightChild.addr, tbm);
//    }

    recomputeInfo();
    // Only notify the father if the message does not come from it
    if (father.addr != CommAddress() && (msg.isFromEN() || father.addr != src)) {
        notify();
    }
}
