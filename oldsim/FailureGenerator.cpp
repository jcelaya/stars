/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
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

#include <algorithm>
#include "FailureGenerator.hpp"
#include "Logger.hpp"
#include "Task.hpp"
#include "Scheduler.hpp"
#include "StructureNode.hpp"
#include "ResourceNode.hpp"
#include "TaskBagMsg.hpp"
#include "AvailabilityInformation.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost;


class FailureMsg : public BasicMsg {
public:
    MESSAGE_SUBCLASS(FailureMsg);

    MSGPACK_DEFINE();
};


void FailureGenerator::startFailures(double mtbf, unsigned int minf, unsigned int maxf, unsigned int mf) {
    meanTime = mtbf;
    minFail = minf;
    maxFail = maxf;
    maxFailures = mf;
    failingNodes.resize(Simulator::getInstance().getNumNodes());
    for (unsigned int i = 0; i < failingNodes.size(); i++)
        failingNodes[i] = i;
    randomFailure();
}


void FailureGenerator::randomFailure() {
    if (maxFailures == 0) return;
    else if (maxFailures != (unsigned int)(-1)) maxFailures--;
    LogMsg("Sim.Fail", DEBUG) << "Generating new failure";
    // Get number of failing nodes
    numFailing = Simulator::uniform(minFail, maxFail, 1);
    Simulator & sim = Simulator::getInstance();
    if (numFailing > sim.getNumNodes()) numFailing = sim.getNumNodes();
    Duration failAt(Simulator::exponential((meanTime * numFailing) / sim.getNumNodes()));
    random_shuffle(failingNodes.begin(), failingNodes.end());
    sim.injectMessage(0, 0, shared_ptr<FailureMsg>(new FailureMsg), failAt);
}


bool FailureGenerator::isNextFailure(const BasicMsg & msg) {
    if (typeid(msg) == typeid(FailureMsg)) {
        Simulator & sim = Simulator::getInstance();
        // nodes fail!!
        LogMsg("Sim.Fail", DEBUG) << numFailing << " nodes FAIL at " << sim.getCurrentTime();

        for (unsigned int i = 0; i < numFailing; i++) {
            unsigned int failed = failingNodes[i];
            LogMsg("Sim.Fail", DEBUG) << "Fails node " << CommAddress(failed, ConfigurationManager::getInstance().getPort());

            // Stop tasks at failed node
            list<shared_ptr<Task> > t;
            t.swap(sim.getNode(failed).getScheduler().getTasks());
            if (!t.empty()) t.front()->abort();

            // Reset availability info at failed node
            list<CommAddress> children;
            sim.setCurrentNode(failed);
            StructureNode & sn = sim.getCurrentNode().getS();
            if (sn.inNetwork()) {
                // Inform nodes about structure changes
                // Delete children and father
                for (unsigned int i = 0; i < sn.getNumChildren(); i++)
                    children.push_back(sn.getSubZone(i)->getLink());
                sn.fireCommitChanges(false, children);
                // Add them again
                sn.fireCommitChanges(true, children);
                // The father, if it exists
                if (sn.getFather() != CommAddress()) {
                    list<CommAddress> thisNode;
                    thisNode.push_back(CommAddress(failed, ConfigurationManager::getInstance().getPort()));
                    sim.setCurrentNode(sn.getFather().getIPNum());
                    // Remove
                    sim.getCurrentNode().getS().fireCommitChanges(false, thisNode);
                    // And add again
                    sim.getCurrentNode().getS().fireCommitChanges(false, thisNode);
                }
                // The children
                for (unsigned int i = 0; i < sn.getNumChildren(); i++) {
                    sim.setCurrentNode(sn.getSubZone(i)->getLink().getIPNum());
                    if (!sn.isRNChildren()) {
                        sim.getCurrentNode().getS().fireCommitChanges(true, list<CommAddress>());
                    } else {
                        sim.getCurrentNode().getE().fireFatherChanged(true);
                    }
                }
            }
        }
        // Program next failure
        randomFailure();
        return true;
    }
    return false;
}
