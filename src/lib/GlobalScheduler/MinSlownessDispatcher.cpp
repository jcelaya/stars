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
#include "MinSlownessDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using std::vector;


void MinSlownessDispatcher::recomputeInfo() {
    LogMsg("Dsp.MS", DEBUG) << "Recomputing the branch information";
    // Only recalculate info for the father
    vector<Link>::iterator child = children.begin();
    for (; child != children.end() && !child->availInfo.get(); child++);
    if (child == children.end()) {
        father.waitingInfo.reset();
        return;
    }
    father.waitingInfo.reset(child->availInfo->clone());
    for (child++; child != children.end(); child++)
        if (child->availInfo.get())
            father.waitingInfo->join(*child->availInfo);
    LogMsg("Dsp.MS", DEBUG) << "The result is " << *father.waitingInfo;

    if(!structureNode.isRNChildren()) {
        for (unsigned int i = 0; i < children.size(); i++) {
            LogMsg("Dsp.MS", DEBUG) << "Recomputing the information from the rest of the tree for child " << i;
            // TODO: send full information, based on configuration switch
            double minSlowness = 0.0;
            bool valid = false;
            if (structureNode.getFather() != CommAddress() && father.availInfo.get()) {
                minSlowness = father.availInfo->getMinimumSlowness();
                valid = true;
            }
            for (unsigned int j = 0; j < children.size(); j++)
                if (j != i && children[j].availInfo.get()) {
                    if (!valid) {
                        minSlowness = children[j].availInfo->getMinimumSlowness();
                        valid = true;
                    }
                    else {
                        if (minSlowness > children[j].availInfo->getMinimumSlowness()) {
                            minSlowness = children[j].availInfo->getMinimumSlowness();
                        }
                    }
                }
            if (valid) {
                children[i].waitingInfo.reset(new SlownessInformation);
                children[i].waitingInfo->setMinimumSlowness(minSlowness);
            }
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


void MinSlownessDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    // TODO: Check that we are not in a change
    if (msg.isForEN()) return;
    LogMsg("Dsp.MS", INFO) << "Received a TaskBagMsg from " << src;
    if (!structureNode.inNetwork()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but not in network";
        return;
    }
    boost::shared_ptr<SlownessInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
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

    unsigned int fLimit[children.size()], branchTasks[children.size()];
    double minSlowness = INFINITY;
    Time now = Time::getCurrentTime();
    // Get the slowness functions of each branch that fulfills memory and disk requirements
    std::vector<std::pair<SlownessInformation::LAFunction *, unsigned int> > functions;
    for (size_t i = 0; i < children.size(); ++i) {
        branchTasks[i] = 0;
        if (children[i].availInfo.get()) {
            LogMsg("Dsp.MS", DEBUG) << "Getting functions of children " << i << " (" << children[i].addr << "): " << *children[i].availInfo;
            children[i].availInfo->updateRkReference(now);
            children[i].availInfo->getFunctions(req, functions);
        }
        fLimit[i] = functions.size();
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
    if (structureNode.getFather() != CommAddress() && (msg.isFromEN() || structureNode.getFather() != src)) {
        // Compare the slowness reached by the new application with the one in the rest of the tree,
        double slownessLimit = 0.0;
        if (father.availInfo.get())
            slownessLimit = father.availInfo->getMinimumSlowness();
        else if (zoneInfo.get())
            slownessLimit = zoneInfo->getMinimumSlowness();
        LogMsg("Dsp.MS", DEBUG) << "The minimum slowness in the rest of the tree is " << slownessLimit;
        slownessLimit *= ConfigurationManager::getInstance().getSlownessRatio();
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
            CommLayer::getInstance().sendMessage(structureNode.getFather(), msg.clone());
            return;
        }
    }

    // Count tasks per branch and update functions
    unsigned int branchNumber = 0;
    for (size_t i = 0; i < tpn.size(); ++i) {
        if (fLimit[branchNumber] == i) ++branchNumber;
        if (tpn[i]) {
            unsigned int tasksToCluster = tpn[i] * functions[i].second;
            if (i == slownessHeap.front().second)
                tasksToCluster -= totalTasks - numTasks;
            branchTasks[branchNumber] += tasksToCluster;
            functions[i].first->update(a, tpn[i]);
        }
    }

    // We are going down!
    // Each branch is sent its accounted number of tasks
    uint32_t nextTask = msg.getFirstTask();
    for (unsigned int child = 0; child < children.size(); ++child) {
        if (branchTasks[child] > 0) {
            LogMsg("Dsp.MS", DEBUG) << "Finally sending " << branchTasks[child] << " tasks to " << children[child].addr;
            TaskBagMsg * tbm = msg.clone();
            tbm->setFromEN(false);
            tbm->setFirstTask(nextTask);
            nextTask += branchTasks[child];
            tbm->setLastTask(nextTask - 1);
            tbm->setForEN(structureNode.isRNChildren());
            CommLayer::getInstance().sendMessage(children[child].addr, tbm);
        }
    }

    recomputeInfo();
    // Only notify the father if the message does not come from it
    if (structureNode.getFather() != CommAddress() && structureNode.getFather() != src) {
        notify();
    }
}
