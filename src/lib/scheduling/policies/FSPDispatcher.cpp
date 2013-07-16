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
              totalTasks(0), diffWithRequest(0), minSlowness(INFINITY),
              branchSlowness{bs[0], bs[1]}, worstChild(0) {
        size_t j = 0;
        totalNodes = 0;
        for (int c : {0, 1}) {
            nodesPerBranch[c] = totalNodes;
            for (auto i : clusters[c]) {
                (*this)[j].f = &i->getMaximumSlowness();
                (*this)[j].v = i->getValue();
                (*this)[j].child = c;
                (*this)[j].slowness = INFINITY;
                (*this)[j].tasks = 0;
                totalNodes += i->getValue();
                ++j;
            }
            nodesPerBranch[c] = totalNodes - nodesPerBranch[c];
        }
    }

    void computeTasksPerFunction(unsigned int numTasksReq, uint64_t a) {
        if (!empty()) {
            minSlowness = 0.0;
            for (int currentTpn = 1; totalTasks < numTasksReq; ++currentTpn) {
                vector<std::pair<double, FunctionInfo *>> slownessHeap;
                for (auto & func : *this) {
                    double slowness = currentTpn == 1 ?
                            func.f->getSlowness(a)
                            : func.f->estimateSlowness(a, currentTpn);
                    // Check that it is not under the minimum of its branch
                    if (slowness < branchSlowness[func.child]) {
                        slowness = branchSlowness[func.child];
                    }
                    slownessHeap.push_back(std::make_pair(slowness, &func));
                    std::push_heap(slownessHeap.begin(), slownessHeap.end(), compare);
                }
                while (!slownessHeap.empty() && totalTasks < numTasksReq) {
                    std::pop_heap(slownessHeap.begin(), slownessHeap.end(), compare);
                    auto & func = *slownessHeap.back().second;
                    func.tasks++;
                    func.slowness = slownessHeap.back().first;
                    minSlowness = func.slowness;
                    worstChild = func.child;
                    totalTasks += func.v;
                    slownessHeap.pop_back();
                }
            }
            diffWithRequest = totalTasks - numTasksReq;
            updateBranchSlowness();
        }
    }

    const double * getNewBranchSlowness() const { return branchSlowness; }

    double getMinimumSlowness() const {
        return minSlowness;
    }

    void computeTasksPerBranch(unsigned int tpb[2]) {
        for (auto & func : *this) {
            if (func.tasks) {
                unsigned int tasksToCluster = func.tasks * func.v;
                tpb[func.child] += tasksToCluster;
            }
        }
        tpb[worstChild] -= diffWithRequest;
    }

    unsigned int getTotalNodes() const {
        return totalNodes;
    }

    unsigned int getNodesOfBranch(int c) const {
        return nodesPerBranch[c];
    }

private:
    unsigned int totalTasks, diffWithRequest, totalNodes;
    unsigned int nodesPerBranch[2];
    double minSlowness;
    double branchSlowness[2];
    int worstChild;

    static bool compare(const std::pair<double, FunctionInfo *> & l, const std::pair<double, FunctionInfo *> & r) {
        return l.first > r.first;
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
};


void FSPDispatcher::informationUpdated() {
    while (!delayedRequests.empty() && validInformation()) {
        LogMsg("Dsp.FSP", INFO) << "Relaunch request";
        TaskBagMsg * msg = delayedRequests.front();
        delayedRequests.pop_front();
        handle(child[0].addr, *msg);
        delete msg;
    }
}


void FSPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    // TODO: Check that we are not in a change
    if (msg.isForEN() || !checkState()) return;
    showMsgSource(src, msg);

    if (!validInformation()) {
        delayedRequests.push_back(msg.clone());
        LogMsg("Dsp.FSP", INFO) << "Invalid information, waiting...";
        return;
    }
//    if (!msg.isFromEN() && (
//            (src == child[0].addr && (!child[0].availInfo.get() || msg.getInfoSequenceUsed() > child[0].availInfo->getSeq())) ||
//            (src == child[1].addr && (!child[1].availInfo.get() || msg.getInfoSequenceUsed() > child[1].availInfo->getSeq())))) {
//        delayedRequests.push_back(msg.clone());
//        LogMsg("Dsp.FSP", INFO) << "Outdated information, waiting...";
//        return;
//    }

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int numTasksReq = msg.getLastTask() - msg.getFirstTask() + 1;
    uint64_t a = req.getLength();

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
    LogMsg("Dsp.FSP", INFO) << "Result minimum slowness is " << functions.getMinimumSlowness();
    double minSlowness = functions.getMinimumSlowness(), slownessLimit = getSlownessLimit();
    unsigned int numTasks[2] = {0, 0};
    functions.computeTasksPerBranch(numTasks);

    if (mustGoDown(src, msg))
        LogMsg("Dsp.FSP", DEBUG) << "The request must go down.";
    if (functions.getNodesOfBranch(0) >= numTasks[0])
        LogMsg("Dsp.FSP", DEBUG) << "There are enough nodes in left branch.";
    if (functions.getNodesOfBranch(1) >= numTasks[1])
        LogMsg("Dsp.FSP", DEBUG) << "There are enough nodes in right branch.";
    if (minSlowness <= slownessLimit)
        LogMsg("Dsp.FSP", DEBUG) << "The slowness is below the limit.";

    if (mustGoDown(src, msg) ||
            (functions.getTotalNodes() >= numTasksReq &&
             minSlowness <= slownessLimit)) {
        LogMsg("Dsp.FSP", DEBUG) << "Sending " << numTasks[0] << " tasks to left child (" << child[0].addr << ")"
                " and " << numTasks[1] << " tasks to right child (" << child[1].addr << ")";
        sendTasks(msg, numTasks, false);

        updateBranchSlowness(functions.getNewBranchSlowness());
        // Invalidate children info
        if (numTasks[0]) child[0].availInfo->reset();
        if (numTasks[1]) child[1].availInfo->reset();
        recomputeInfo();
        // Only notify the father if the message does not come from it
        if (!mustGoDown(src, msg)) {
            notify();
        }
    } else {
        LogMsg("Dsp.FSP", INFO) << "Not enough information to route this request, sending to the father.";
        CommLayer::getInstance().sendMessage(father.addr, msg.getSubRequest(msg.getFirstTask(), msg.getLastTask()));
    }
}


double FSPDispatcher::getSlownessLimit() const {
    boost::shared_ptr<FSPAvailabilityInformation> zoneInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
    // Compare the slowness reached by the new application with the one in the rest of the tree,
    double slownessLimit = zoneInfo->getMaximumSlowness();
    LogMsg("Dsp.FSP", DEBUG) << "The maximum slowness in this branch is " << slownessLimit;
    if (father.availInfo.get()) {
        slownessLimit = father.availInfo->getMaximumSlowness();
        LogMsg("Dsp.FSP", DEBUG) << "The maximum slowness in the rest of the tree is " << slownessLimit;
    }
    LogMsg("Dsp.FSP", DEBUG) << "The slowest machine in this branch would provide a slowness of " << zoneInfo->getSlowestMachine();
    if (zoneInfo->getSlowestMachine() > slownessLimit)
        slownessLimit = zoneInfo->getSlowestMachine();
    slownessLimit *= beta;
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
