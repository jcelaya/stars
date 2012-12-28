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
            double fatherMinSlowness = father.availInfo->getMinimumSlowness();
            leftChild.waitingInfo.reset(new MSPAvailabilityInformation);
            if (rightChild.availInfo.get()) {
                double rightMinSlowness = rightChild.availInfo->getMinimumSlowness();
                leftChild.waitingInfo->setMinimumSlowness(fatherMinSlowness > rightMinSlowness ? fatherMinSlowness : rightMinSlowness);
            } else
                leftChild.waitingInfo->setMinimumSlowness(fatherMinSlowness);
        } else if (rightChild.availInfo.get()) {
            leftChild.waitingInfo.reset(new MSPAvailabilityInformation);
            leftChild.waitingInfo->setMinimumSlowness(rightChild.availInfo->getMinimumSlowness());
        } else
            leftChild.waitingInfo.reset();
    }

    if(!branch.isRightLeaf()) {
        LogMsg("Dsp.MS", DEBUG) << "Recomputing the information from the rest of the tree for right child.";
        // TODO: send full information, based on configuration switch
        if (father.availInfo.get()) {
            double fatherMinSlowness = father.availInfo->getMinimumSlowness();
            rightChild.waitingInfo.reset(new MSPAvailabilityInformation);
            if (leftChild.availInfo.get()) {
                double leftMinSlowness = leftChild.availInfo->getMinimumSlowness();
                rightChild.waitingInfo->setMinimumSlowness(fatherMinSlowness > leftMinSlowness ? fatherMinSlowness : leftMinSlowness);
            } else
                rightChild.waitingInfo->setMinimumSlowness(fatherMinSlowness);
        } else if (leftChild.availInfo.get()) {
            rightChild.waitingInfo.reset(new MSPAvailabilityInformation);
            rightChild.waitingInfo->setMinimumSlowness(leftChild.availInfo->getMinimumSlowness());
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
    // TODO: Check that we are not in a change
    if (msg.isForEN()) return;
    LogMsg("Dsp.MS", INFO) << "Received a TaskBagMsg from " << src;
    if (!branch.inNetwork()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but not in network";
        return;
    }
    boost::shared_ptr<MSPAvailabilityInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
    if (!zoneInfo.get()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but no information!";
        return;
    }

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int numTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    uint64_t a = req.getLength();
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

    LogMsg("Dsp.MS", DEBUG) << "Result minimum slowness is " << minSlowness;

    // if we are not the root and the message does not come from the father
    if (father.addr != CommAddress() && (msg.isFromEN() || father.addr != src)) {
        // Compare the slowness reached by the new application with the one in the rest of the tree,
        double slownessLimit = 0.0;
        if (father.availInfo.get())
            slownessLimit = father.availInfo->getMinimumSlowness();
        else if (zoneInfo.get())
            slownessLimit = zoneInfo->getMinimumSlowness();
        LogMsg("Dsp.MS", DEBUG) << "The minimum slowness in the rest of the tree is " << slownessLimit;
        slownessLimit *= beta;
        if (zoneInfo.get()) {
            LogMsg("Dsp.MS", DEBUG) << "The maximum slowness in this branch is " << zoneInfo->getMaximumSlowness();
            if (zoneInfo->getMaximumSlowness() > slownessLimit)
                slownessLimit = zoneInfo->getMaximumSlowness();
            LogMsg("Dsp.MS", DEBUG) << "The slowest machine in this branch would provide a slowness of " << zoneInfo->getSlowestMachine();
            if (zoneInfo->getSlowestMachine() > slownessLimit)
                slownessLimit = zoneInfo->getSlowestMachine();
        }
        if (minSlowness > slownessLimit) {
            LogMsg("Dsp.MS", INFO) << "Not enough information to route this request, sending to the father.";
            CommLayer::getInstance().sendMessage(father.addr, msg.clone());
            return;
        }
    }

    // Count tasks per branch and update functions
    unsigned int leftTasks = 0, rightTasks = 0;
    for (size_t i = 0; i < rfStart; ++i) {
        if (tpn[i]) {
            unsigned int tasksToCluster = tpn[i] * functions[i].second;
            if (i == slownessHeap.front().second)
                tasksToCluster -= totalTasks - numTasks;
            leftTasks += tasksToCluster;
            functions[i].first->update(a, tpn[i]);
        }
    }

    for (size_t i = rfStart; i < tpn.size(); ++i) {
        if (tpn[i]) {
            unsigned int tasksToCluster = tpn[i] * functions[i].second;
            if (i == slownessHeap.front().second)
                tasksToCluster -= totalTasks - numTasks;
            rightTasks += tasksToCluster;
            functions[i].first->update(a, tpn[i]);
        }
    }

    // We are going down!
    // Each branch is sent its accounted number of tasks
    sendTasks(msg, leftTasks, rightTasks, false);

    recomputeInfo();
    // Only notify the father if the message does not come from it
    if (father.addr != CommAddress() && father.addr != src) {
        notify();
    }
}
