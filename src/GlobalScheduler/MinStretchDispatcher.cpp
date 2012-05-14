/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
 *
 *  This file is part of PeerComp.
 *
 *  PeerComp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  PeerComp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PeerComp; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "Logger.hpp"
#include "MinStretchDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using boost::shared_ptr;


void MinStretchDispatcher::recomputeInfo() {
    LogMsg("Dsp.MS", DEBUG) << "Recomputing the branch information";
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
    LogMsg("Dsp.MS", DEBUG) << "The result is " << *father.waitingInfo;

    if (!structureNode.isRNChildren()) {
        for (unsigned int i = 0; i < children.size(); i++) {
            LogMsg("Dsp.MS", DEBUG) << "Recomputing the information from the rest of the tree for child " << i;
            // TODO: send full information, based on configuration switch
            double minStretch = 0.0, maxStretch = 0.0;
            bool valid = false;
            if (structureNode.getFather() != CommAddress() && father.availInfo.get()) {
                minStretch = father.availInfo->getMinimumStretch();
                maxStretch = father.availInfo->getMaximumStretch();
                valid = true;
            }
            for (unsigned int j = 0; j < children.size(); j++)
                if (j != i && children[j].availInfo.get()) {
                    if (!valid) {
                        minStretch = children[j].availInfo->getMinimumStretch();
                        maxStretch = children[j].availInfo->getMaximumStretch();
                        valid = true;
                    } else {
                        if (minStretch > children[j].availInfo->getMinimumStretch()) {
                            minStretch = children[j].availInfo->getMinimumStretch();
                        }
                        if (maxStretch < children[j].availInfo->getMaximumStretch()) {
                            maxStretch = children[j].availInfo->getMaximumStretch();
                        }
                    }
                }
            if (valid && !(children[i].waitingInfo.get()
                           && children[i].waitingInfo->getMinimumStretch() == minStretch
                           && children[i].waitingInfo->getMaximumStretch() == maxStretch)) {
                LogMsg("Dsp.MS", DEBUG) << "There were changes with children " << i;
                children[i].waitingInfo.reset(new StretchInformation);
                children[i].waitingInfo->setMinAndMaxStretch(minStretch, maxStretch);
            }
        }
    }
}


struct SpecificAFPtr {
    SpecificAFPtr(StretchInformation::SpecificAF * _f) : f(_f) {}
    bool operator<(const SpecificAFPtr & r) const {
        return f->getCurrentStretch() > r.f->getCurrentStretch();
    }
    StretchInformation::SpecificAF * f;
};


void MinStretchDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    // TODO: Check that we are not in a change
    if (msg.isForEN()) return;
    LogMsg("Dsp.MS", INFO) << "Received a TaskBagMsg from " << src;
    if (!structureNode.inNetwork()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but not in network";
        return;
    }
    if (!father.waitingInfo.get()) {
        LogMsg("Dsp.MS", WARN) << "TaskBagMsg received but no information!";
        return;
    }

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int numTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    LogMsg("Dsp.MS", INFO) << "Requested allocation of application " << msg.getRequestId() << " with " << numTasks << " tasks with requirements:";
    LogMsg("Dsp.MS", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk();
    LogMsg("Dsp.MS", INFO) << "Length: " << req.getLength();

    // TODO: If it is comming from the father, update its notified information
// if (!msg.isFromEN() && structureNode.getFather() == src && father.availInfo.get()) {
//  father.availInfo->update(numTasks, req.getLength());
// }

    // Calculate the number of available slots for this application with beta times the minimum stretch,
    // if we are not the root and the message does not come from the father
    if (structureNode.getFather() != CommAddress() && (msg.isFromEN() || structureNode.getFather() != src)) {
        double minStretchRest = 0.0;
        if (father.availInfo.get())
            minStretchRest = father.availInfo->getMinimumStretch();
        else if (father.waitingInfo.get())
            minStretchRest = father.waitingInfo->getMinimumStretch();
        LogMsg("Dsp.MS", DEBUG) << "The minimum stretch in the rest of the tree is " << minStretchRest;
        if (!father.waitingInfo.get() ||
                numTasks > father.waitingInfo->getAvailableSlots(req, ConfigurationManager::getInstance().getStretchRatio() * minStretchRest)) {
            LogMsg("Dsp.MS", INFO) << "Not enough information to route this request, sending to the father.";
            CommLayer::getInstance().sendMessage(structureNode.getFather(), msg.clone());
            return;
        }
    }

    // We are going down!
    unsigned int totalTasks = 0, branchTasks[children.size()];

    // Get the functions of each branch that fulfills memory and disk requirements
    std::list<StretchInformation::SpecificAF> specificFunctions;
    std::list<StretchInformation::SpecificAF>::iterator start = specificFunctions.begin();
    std::vector<std::pair<SpecificAFPtr, int> > sfHeap;
    for (unsigned int i = 0; i < children.size(); ++i) {
        branchTasks[i] = 0;
        if (children[i].availInfo.get()) {
            children[i].availInfo->getSpecificFunctions(req, specificFunctions);
            if (start == specificFunctions.end())
                start = specificFunctions.begin();
            else ++start;
            sfHeap.reserve(specificFunctions.size());
            for (std::list<StretchInformation::SpecificAF>::iterator it = start; it != specificFunctions.end(); start = it++)
                sfHeap.push_back(std::make_pair(SpecificAFPtr(&(*it)), i));
        }
    }
    if (sfHeap.empty()) {
        // TODO: we can simply divide the request in two, instead of failing
        LogMsg("Dsp.MS", WARN) << "Not enough information to route this request, and cannot send to the father, discarding!";
        return;
    }

    // Sort them in a heap, from lowest to highest stretch
    std::make_heap(sfHeap.begin(), sfHeap.end());

    double minStretch;
    while (totalTasks < numTasks) {
        std::pop_heap(sfHeap.begin(), sfHeap.end());
        unsigned int deltaTasks = sfHeap.back().first.f->getNumNodes();
        // Adjust the number of tasks in the last updated branch
        if (deltaTasks > numTasks - totalTasks) deltaTasks = numTasks - totalTasks;
        // Account for the total number of tasks and the total in its branch.
        totalTasks += deltaTasks;
        branchTasks[sfHeap.back().second] += deltaTasks;
        minStretch = sfHeap.back().first.f->getCurrentStretch();
        // Advance the counter and push the function into the heap again.
        sfHeap.back().first.f->step();
        std::push_heap(sfHeap.begin(), sfHeap.end());
    }

    LogMsg("Dsp.MS", DEBUG) << "Result minimum stretch is " << minStretch;

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
            // TODO: Update the zone information to avoid sending more tasks there
            //children[child].second->update(assignedTasks[child], req.getLength());
        }
    }
    // TODO: Send updated information upwards

    // TODO
// recomputeInfo();
// // Only notify the father if the message does not come from it
// if (structureNode.getFather() != CommAddress() && structureNode.getFather() != src) {
//  notify();
// }
}
