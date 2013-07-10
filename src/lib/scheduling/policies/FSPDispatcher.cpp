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


struct FunctionInfo {
    const ZAFunction * f;
    unsigned int v;
    int child;
    double slowness;
    int tasks;
};


class FunctionVector : public std::vector<FunctionInfo> {
public:
    FunctionVector(std::list<FSPAvailabilityInformation::MDLCluster *> clusters[2], double bs[2])
            : std::vector<FunctionInfo>(clusters[0].size() + clusters[1].size()),
              totalTasks(0), diffWithRequest(0), branchSlowness{bs[0], bs[1]} {
        size_t j = 0;
        for (int c : {0, 1}) {
            for (auto i : clusters[c]) {
                (*this)[j].f = &i->getMaximumSlowness();
                (*this)[j].v = i->getValue();
                (*this)[j].child = c;
                (*this)[j].slowness = INFINITY;
                (*this)[j].tasks = 0;
                ++j;
            }
        }
    }

    void computeTasksPerFunction(unsigned int numTasksReq, uint64_t a) {
        if (!empty()) {
            bool tryOneMoreTask = true;
            for (int currentTpn = 1; tryOneMoreTask; ++currentTpn) {
                // Try with one more task per node
                tryOneMoreTask = false;
                for (auto & func : *this) {
                    // With those functions that got an additional task per node in the last iteration,
                    if (func.tasks == currentTpn - 1) {
                        // calculate the slowness with one task more
                        double slowness = currentTpn == 1 ?
                                func.f->getSlowness(a)
                                : func.f->estimateSlowness(a, currentTpn);
                        // Check that it is not under the minimum of its branch
                        if (slowness < branchSlowness[func.child]) {
                            slowness = branchSlowness[func.child];
                        }
                        // If the slowness is less than the maximum to date, or there aren't enough tasks yet, insert it in the heap
                        if (totalTasks < numTasksReq || slowness < slownessHeap.front()->slowness) {
                            // Add one task per node to that function
                            ++func.tasks;
                            func.slowness = slowness;
                            slownessHeap.push_back(&func);
                            std::push_heap(slownessHeap.begin(), slownessHeap.end());
                            totalTasks += func.v;
                            removeExceedingTasks(numTasksReq);
                            // As long as one function gets another task, we continue trying
                            tryOneMoreTask = true;
                        }
                    }
                }
            }
            diffWithRequest = totalTasks - numTasksReq;
            updateBranchSlowness();
        }
    }

    const double * getNewBranchSlowness() const { return branchSlowness; }

    double getMinimumSlowness() const {
        return empty() ? INFINITY : slownessHeap.front()->slowness;
    }

    void computeTasksPerBranch(unsigned int tpb[2]) {
        for (auto & func : *this) {
            if (func.tasks) {
                unsigned int tasksToCluster = func.tasks * func.v;
                if (&func == slownessHeap.front()) {
                    tasksToCluster -= diffWithRequest;
                }
                tpb[func.child] += tasksToCluster;
                //func.f->update(a, tpn[i]);
            }
        }
    }

private:
    unsigned int totalTasks, diffWithRequest;
    vector<FunctionInfo *> slownessHeap;
    double branchSlowness[2];

    static bool compare(FunctionInfo * l, FunctionInfo * r) {
        return l->slowness < r->slowness;
    }

    void updateBranchSlowness() {
        for (auto & func : *this) {
            if (func.tasks) {
                if (branchSlowness[func.child] < func.slowness) {
                    branchSlowness[func.child] = func.slowness;
                }
            }
        }
    }

    void removeExceedingTasks(unsigned int numTasksReq) {
        while (totalTasks - slownessHeap.front()->v >= numTasksReq) {
            totalTasks -= slownessHeap.front()->v;
            --slownessHeap.front()->tasks;
            std::pop_heap(slownessHeap.begin(), slownessHeap.end());
            slownessHeap.pop_back();
        }
    }
};


void FSPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    // TODO: Check that we are not in a change
    if (msg.isForEN() || !checkState()) return;
    showMsgSource(src, msg);

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int numTasksReq = msg.getLastTask() - msg.getFirstTask() + 1;
    uint64_t a = req.getLength();

    // If it is comming from the father, update its notified information
//	if (!msg.isFromEN() && structureNode.getFather() == src && father.availInfo.get()) {
//		father.availInfo->update(numTasks, req.getLength());
//	}

    std::list<FSPAvailabilityInformation::MDLCluster *> clusters[2];
    double branchSlowness[2] = {0.0, 0.0};
    for (int c : {0, 1}) {
        if (child[c].availInfo.get()) {
            LogMsg("Dsp.FSP", DEBUG) << "Getting functions of children (" << child[c].addr << "): " << *child[c].availInfo;
            clusters[c] = std::move(child[c].availInfo->getFunctions(req));
            branchSlowness[c] = child[c].availInfo->getMinimumSlowness();
        }
    }
    FunctionVector functions(clusters, branchSlowness);
    functions.computeTasksPerFunction(numTasksReq, a);
    double minSlowness = functions.getMinimumSlowness();
    LogMsg("Dsp.FSP", INFO) << "Result minimum slowness is " << minSlowness;

    if (minSlowness <= getSlownessLimit(src, msg)) {
        unsigned int numTasks[2] = {0, 0};
        functions.computeTasksPerBranch(numTasks);
        LogMsg("Dsp.FSP", DEBUG) << "Sending " << numTasks[0] << " tasks to left child (" << child[0].addr << ")"
                " and " << numTasks[1] << " tasks to right child (" << child[1].addr << ")";
        sendTasks(msg, numTasks, false);

        updateBranchSlowness(functions.getNewBranchSlowness());
        recomputeInfo();
        // Only notify the father if the message does not come from it
        if (father.addr != CommAddress() && (msg.isFromEN() || father.addr != src)) {
            notify();
        }
    } else {
        LogMsg("Dsp.FSP", INFO) << "Not enough information to route this request, sending to the father.";
        CommLayer::getInstance().sendMessage(father.addr, msg.clone());
    }
}


double FSPDispatcher::getSlownessLimit(const CommAddress & src, const TaskBagMsg & msg) const {
    double slownessLimit = INFINITY;
    // if we are not the root and the message does not come from the father
    if (father.addr != CommAddress() && (msg.isFromEN() || father.addr != src)) {
        boost::shared_ptr<FSPAvailabilityInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
        // Compare the slowness reached by the new application with the one in the rest of the tree,
        slownessLimit = zoneInfo->getMaximumSlowness();
        LogMsg("Dsp.FSP", DEBUG) << "The maximum slowness in this branch is " << slownessLimit;
        if (father.availInfo.get()) {
            slownessLimit = father.availInfo->getMaximumSlowness();
            LogMsg("Dsp.FSP", DEBUG) << "The maximum slowness in the rest of the tree is " << slownessLimit;
        }
        LogMsg("Dsp.FSP", DEBUG) << "The slowest machine in this branch would provide a slowness of " << zoneInfo->getSlowestMachine();
        if (zoneInfo->getSlowestMachine() > slownessLimit)
            slownessLimit = zoneInfo->getSlowestMachine();
        slownessLimit *= beta;
    }
    return slownessLimit;
}


void FSPDispatcher::updateBranchSlowness(const double branchSlowness[2]) {
    for (int c : {0, 1}) {
        if (child[c].availInfo.get()) {
            child[c].availInfo->setMinimumSlowness(branchSlowness[c]);
            if (child[c].availInfo->getMaximumSlowness() < branchSlowness[c]) {
                child[c].availInfo->setMaximumSlowness(branchSlowness[c]);
            }
        }
    }
}


void FSPDispatcher::recomputeInfo() {
    LogMsg("Dsp.FSP", DEBUG) << "Recomputing the branch information";
    // Recalculate info for the father
    recomputeFatherInfo();

    for (int c : {0, 1}) {
        if(!branch.isLeaf(c)) {
            LogMsg("Dsp.FSP", DEBUG) << "Recomputing the information from the rest of the tree for " << c << " child.";
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
