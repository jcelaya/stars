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
#include "FSPDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using std::vector;

using stars::ZAFunction;

double FSPDispatcher::beta = 2.0;
//int FSPDispatcher::estimations = 0;


void FSPDispatcher::recomputeInfo() {
    LogMsg("Dsp.MS", DEBUG) << "Recomputing the branch information";
    // Recalculate info for the father
    recomputeFatherInfo();

    for (int c : {0, 1}) {
        if(!branch.isLeaf(c)) {
            LogMsg("Dsp.MS", DEBUG) << "Recomputing the information from the rest of the tree for " << c << " child.";
            // TODO: send full information, based on configuration switch
            if (father.availInfo.get()) {
                double fatherMaxSlowness = father.availInfo->getMaximumSlowness();
                child[c].waitingInfo.reset(new FSPAvailabilityInformation);
                if (child[c^1].availInfo.get()) {
                    double rightMaxSlowness = child[c^1].availInfo->getMaximumSlowness();
                    child[c].waitingInfo->setMaximumSlowness(fatherMaxSlowness > rightMaxSlowness ? fatherMaxSlowness : rightMaxSlowness);
                } else
                    child[c].waitingInfo->setMaximumSlowness(fatherMaxSlowness);
            } else if (child[c^1].availInfo.get()) {
                child[c].waitingInfo.reset(new FSPAvailabilityInformation);
                child[c].waitingInfo->setMaximumSlowness(child[c^1].availInfo->getMaximumSlowness());
            } else
                child[c].waitingInfo.reset();
        }
    }
}


struct SlownessTasks {
    double slowness;
    int numTasks;
    unsigned int i;
    void set(double s, int n, int index) { slowness = s; numTasks = n; i = index; }
    bool operator<(const SlownessTasks & o) const { return slowness < o.slowness || (slowness == o.slowness && numTasks < o.numTasks); }
};


void FSPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN()) return;

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int numTasksReq = msg.getLastTask() - msg.getFirstTask() + 1;
    uint64_t a = req.getLength();

    // TODO: Check that we are not in a change
    if (!msg.isFromEN() && src == father.addr) {
        LogMsg("Dsp.MS", INFO) << "Received a TaskBagMsg from " << src << " (father)";
    } else {
        LogMsg("Dsp.MS", INFO) << "Received a TaskBagMsg from " << src << " (" << (src == child[0].addr ? "left child)" : "right child)");
    }
    if (!branch.inNetwork()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but not in network";
        return;
    }
    boost::shared_ptr<FSPAvailabilityInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
    if (!zoneInfo.get()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but no information!";
        return;
    }

    LogMsg("Dsp.MS", INFO) << "Requested allocation of request " << msg.getRequestId() << " with " << numTasksReq << " tasks with requirements:";
    LogMsg("Dsp.MS", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk() << "   Length: " << a;

    // If it is comming from the father, update its notified information
//	if (!msg.isFromEN() && structureNode.getFather() == src && father.availInfo.get()) {
//		father.availInfo->update(numTasks, req.getLength());
//	}

    unsigned int numFunctions[2];
    // Get the slowness functions of each branch that fulfills memory and disk requirements
    std::vector<std::pair<ZAFunction *, unsigned int> > functions;
    for (int c : {0, 1}) {
        if (child[c].availInfo.get()) {
            LogMsg("Dsp.MS", DEBUG) << "Getting functions of left children (" << child[c].addr << "): " << *child[c].availInfo;
            child[c].availInfo->getFunctions(req, functions);
        }
        numFunctions[c] = functions.size();
    }

    unsigned int totalTasks = 0;
    // Number of tasks sent to each node
    vector<int> tpn(functions.size(), 0);
    // Heap of slowness values
    vector<std::pair<double, size_t> > slownessHeap;

    double minSlowness = INFINITY;
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
                    double branchSlowness = (f < numFunctions[0] ? child[0].availInfo : child[1].availInfo)->getMinimumSlowness();
                    if (slowness < branchSlowness) {
                        slowness = branchSlowness;
                    }
                    // If the slowness is less than the maximum to date, or there aren't enough tasks yet, insert it in the heap
                    if (totalTasks < numTasksReq || slowness < slownessHeap.front().first) {
                        slownessHeap.push_back(std::make_pair(slowness, f));
                        std::push_heap(slownessHeap.begin(), slownessHeap.end());
                        // Add one task per node to that function
                        ++tpn[f];
                        totalTasks += functions[f].second;
                        // Remove the highest slowness values as long as there are enough tasks
                        while (totalTasks - functions[slownessHeap.front().second].second >= numTasksReq) {
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
    unsigned int numTasks[2] = {0, 0};
    double branchSlowness[2];
    size_t i = 0;
    for (int c : {0, 1}) {
        branchSlowness[c] = child[c].availInfo.get() ? child[c].availInfo->getMinimumSlowness() : 0.0;

        for (; i < numFunctions[c]; ++i) {
            if (tpn[i]) {
                double slowness = tpn[i] == 1 ?
                        functions[i].first->getSlowness(a)
                        : functions[i].first->estimateSlowness(a, tpn[i]);
                if (branchSlowness[c] < slowness) {
                    branchSlowness[c] = slowness;
                }
                unsigned int tasksToCluster = tpn[i] * functions[i].second;
                if (i == slownessHeap.front().second) {
                    tasksToCluster -= totalTasks - numTasksReq;
                }
                numTasks[c] += tasksToCluster;
                functions[i].first->update(a, tpn[i]);
            }
        }

        if (child[c].availInfo.get()) {
            child[c].availInfo->setMinimumSlowness(branchSlowness[c]);
            if (child[c].availInfo->getMaximumSlowness() < branchSlowness[c]) {
                child[c].availInfo->setMaximumSlowness(branchSlowness[c]);
            }
        }
    }

    LogMsg("Dsp.MS", DEBUG) << "Sending " << numTasks[0] << " tasks to left child (" << child[0].addr << ")"
            " and " << numTasks[1] << " tasks to right child (" << child[1].addr << ")";

    // We are going down!
    // Each branch is sent its accounted number of tasks
    sendTasks(msg, numTasks, false);

    recomputeInfo();
    // Only notify the father if the message does not come from it
    if (father.addr != CommAddress() && (msg.isFromEN() || father.addr != src)) {
        notify();
    }
}
