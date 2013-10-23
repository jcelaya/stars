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
#include "FSPTaskBagMsg.hpp"
using std::vector;
using stars::ZAFunction;

double FSPDispatcher::beta = 0.4;

namespace stars {
REGISTER_MESSAGE(FSPTaskBagMsg);
}

struct FunctionInfo {
    FSPAvailabilityInformation::MDZCluster * cluster;
    int child;
    double slowness;
    int tasks;
};


class FSPDispatcher::FunctionVector : public std::vector<FunctionInfo> {
public:
    FunctionVector(std::list<FSPAvailabilityInformation::MDZCluster *> clusters[2], const std::array<double, 2> & bs)
            : std::vector<FunctionInfo>(clusters[0].size() + clusters[1].size()),
              totalTasks(0), diffWithRequest(0), minSlowness(INFINITY),
              branchSlowness(bs), worstChild(0) {
        size_t j = 0;
        totalNodes = 0;
        for (int c : {0, 1}) {
            nodesPerBranch[c] = totalNodes;
            for (auto i : clusters[c]) {
                (*this)[j].cluster = i;
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
                            func.cluster->getMaximumSlowness().getSlowness(a)
                            : func.cluster->getMaximumSlowness().estimateSlowness(a, currentTpn);
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
                    totalTasks += func.cluster->getValue();
                    slownessHeap.pop_back();
                }
            }
            diffWithRequest = totalTasks - numTasksReq;
            updateBranchSlowness();
        }
    }

    const std::array<double, 2> & getNewBranchSlowness() const { return branchSlowness; }

    double getMinimumSlowness() const {
        return minSlowness;
    }

    std::array<unsigned int, 2> computeTasksPerBranch() {
        std::array<unsigned int, 2> tpb = {0, 0};
        for (auto & func : *this) {
            if (func.tasks) {
                unsigned int tasksToCluster = func.tasks * func.cluster->getValue();
                tpb[func.child] += tasksToCluster;
            }
        }
        tpb[worstChild] -= diffWithRequest;
        return tpb;
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
    std::array<double, 2> branchSlowness;
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
}


void FSPDispatcher::removeUsedClusters(const FunctionVector& functions) {
    std::list<FSPAvailabilityInformation::MDZCluster*> clusters[2];
    for (auto & func : functions) {
        if (func.tasks) {
            clusters[func.child].push_back(func.cluster);
        }
    }
    for (int c : { 0, 1 }) {
        if (!clusters[c].empty()) {
            child[c].availInfo->removeClusters(clusters[c]);
            child[c].hasNewInformation = true;
        }
    }
}


// TODO: Provisional
bool FSPDispatcher::discard = false;
double FSPDispatcher::discardRatio = 2.0;


void FSPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    // TODO: Check that we are not in a change
    if (msg.isForEN() || !checkState()) return;
    showMsgSource(src, msg);

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int numTasksReq = msg.getLastTask() - msg.getFirstTask() + 1;
    uint64_t a = req.getLength();

    std::unique_ptr<stars::FSPTaskBagMsg> tmp;
    const stars::FSPTaskBagMsg * castedMsg = dynamic_cast<const stars::FSPTaskBagMsg *>(&msg);
    if (castedMsg) {
        tmp.reset(castedMsg->clone());
    } else {
        tmp.reset(new stars::FSPTaskBagMsg(msg));
    }

    std::list<FSPAvailabilityInformation::MDZCluster *> clusters[2];
    std::array<double, 2> branchSlowness = {0.0, 0.0};
    for (int c : {0, 1}) {
        if (child[c].availInfo.get()) {
            Logger::msg("Dsp.FSP", DEBUG, "Getting functions of children (", child[c].addr, "): ", *child[c].availInfo);
            clusters[c] = std::move(child[c].availInfo->getFunctions(req));
            branchSlowness[c] = child[c].availInfo->getMinimumSlowness();
        }
    }
    FunctionVector functions(clusters, branchSlowness);
    std::array<unsigned int, 2> numTasks = {0, 0};
    if (!mustGoDown(src, msg) && functions.getTotalNodes() < numTasksReq) {
        Logger::msg("Dsp.FSP", INFO, "Not enough nodes to route this request, sending to the father.");
    } else {
        functions.computeTasksPerFunction(numTasksReq, a);
        double minSlowness = functions.getMinimumSlowness(), slownessLimit = getSlownessLimit();
        Logger::msg("Dsp.FSP", INFO, "Result minimum slowness is ", minSlowness);

        if (mustGoDown(src, msg) || minSlowness <= slownessLimit) {

            if (msg.isFromEN() || src != father.addr) {
                Logger::msg("Dsp.FSP", WARN, "Setting the slowness of the request to ", minSlowness);
                tmp->setEstimatedSlowness(minSlowness);
            }

            Logger::msg("Dsp.FSP", WARN, "Estimation difference: ", minSlowness, ' ', tmp->getEstimatedSlowness());
            if (minSlowness > tmp->getEstimatedSlowness() * discardRatio) {
                if (discard) {
                    Logger::msg("Dsp.FSP", WARN, "Discard tasks, because slowness is much greater than expected: ",
                            minSlowness, " >> ", tmp->getEstimatedSlowness());
                    return;
                } else {
                    Logger::msg("Dsp.FSP", WARN, "Return tasks up, because slowness is much greater than expected: ",
                            minSlowness, " >> ", tmp->getEstimatedSlowness());
                }
            } else {
                numTasks = functions.computeTasksPerBranch();
                updateBranchSlowness(functions.getNewBranchSlowness());
                removeUsedClusters(functions);
                recomputeInfo();
                if (mustGoDown(src, msg)) {
                    Logger::msg("Dsp.FSP", DEBUG, "The request must go down.");
                } else {
                    Logger::msg("Dsp.FSP", DEBUG, "The slowness is below the limit ", slownessLimit);
                    // Only notify the father if the message does not come from it
                    notify();
                }
            }
        } else {
            Logger::msg("Dsp.FSP", INFO, "Not enough information to route this request, sending to the father.");
        }
    }
    sendTasks(*tmp, numTasks, false);
}


double FSPDispatcher::getSlownessLimit() const {
    boost::shared_ptr<FSPAvailabilityInformation> zoneInfo =
            boost::static_pointer_cast<FSPAvailabilityInformation>(getBranchInfo());
    // Compare the slowness reached by the new application with the one in the rest of the tree,
    double slownessLimit = zoneInfo->getMaximumSlowness();
    Logger::msg("Dsp.FSP", DEBUG, "The maximum slowness in this branch is ", slownessLimit);
    if (father.availInfo.get()) {
        slownessLimit = father.availInfo->getMaximumSlowness();
        Logger::msg("Dsp.FSP", DEBUG, "The maximum slowness in the rest of the tree is ", slownessLimit);
    }
    Logger::msg("Dsp.FSP", DEBUG, "The slowest machine in this branch would provide a slowness of ", zoneInfo->getSlowestMachine());
    if (zoneInfo->getSlowestMachine() > slownessLimit)
        slownessLimit = zoneInfo->getSlowestMachine();
    slownessLimit *= beta;
    return slownessLimit;
}


void FSPDispatcher::updateBranchSlowness(const std::array<double, 2> & branchSlowness) {
    for (int c : {0, 1}) {
        if (child[c].availInfo.get()) {
            child[c].availInfo->setMinimumSlowness(branchSlowness[c]);
            if (child[c].availInfo->getMaximumSlowness() < branchSlowness[c]) {
                child[c].availInfo->setMaximumSlowness(branchSlowness[c]);
            }
        }
    }
}


void FSPDispatcher::recomputeChildrenInfo() {
    Logger::msg("Dsp.FSP", DEBUG, "Recomputing the branch information");
    for (int c : {0, 1}) {
        if(!branch.isLeaf(c)) {
            Logger::msg("Dsp.FSP", DEBUG, "Recomputing the information from the rest of the tree for ", c, " child.");
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
