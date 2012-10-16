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

    EMPTY_MSGPACK_DEFINE();
};


void FailureGenerator::startFailures(double median_session, unsigned int minf, unsigned int maxf) {
    unsigned int numNodes = Simulator::getInstance().getNumNodes();
    meanTime = median_session / (numNodes * log(2.0));
    minFail = minf;
    maxFail = maxf > numNodes ? numNodes : maxf;
    numFailures = numFailed = 0;
    unsigned int numFailing = Simulator::uniform(minFail, maxFail, 1);
    programFailure(Duration(Simulator::exponential(meanTime * numFailing)), numFailing);
}


void FailureGenerator::bigFailure(Duration failAt, unsigned int minf, unsigned int maxf) {
    unsigned int numNodes = Simulator::getInstance().getNumNodes();
    minFail = minf;
    maxFail = maxf > numNodes ? numNodes : maxf;
    numFailures = 1;
    numFailed = 0;
    unsigned int numFailing = Simulator::uniform(minFail, maxFail, 1);
    programFailure(failAt, numFailing);
}


void FailureGenerator::programFailure(Duration failAt, unsigned int numFailing) {
    LogMsg("Sim.Fail", DEBUG) << "Generating new failure";
    Simulator & sim = Simulator::getInstance();
    // Simulate a random shuffle
    failingNodes.resize(numFailing);
    for (size_t i = 0; i < sim.getNumNodes(); ++i) {
        size_t pos = Simulator::uniform(0, i, 1);
        if (pos < numFailing) {
            if (i < numFailing && pos != i) failingNodes[i] = failingNodes[pos];
            failingNodes[pos] = i;
        }
    }
    sim.injectMessage(0, 0, shared_ptr<FailureMsg>(new FailureMsg), failAt);
}


bool FailureGenerator::isNextFailure(const BasicMsg & msg) {
    if (typeid(msg) == typeid(FailureMsg)) {
        ++numFailed;
        Simulator & sim = Simulator::getInstance();
        // nodes fail!!
        LogMsg("Sim.Fail", DEBUG) << failingNodes.size() << " nodes FAIL at " << sim.getCurrentTime();
        map<uint32_t, pair<bool, list<CommAddress> > > sneighbours;
        map<uint32_t, bool> rneighbours;
        for (unsigned int i = 0; i < failingNodes.size(); ++i) {
            StarsNode & node = sim.getNode(failingNodes[i]);
            sim.setCurrentNode(failingNodes[i]);
            sneighbours[node.getE().getFather().getIPNum()].second.push_back(node.getLocalAddress());
            if (node.getS().inNetwork()) {
                if (node.getS().getFather() != CommAddress())
                    sneighbours[node.getS().getFather().getIPNum()].second.push_back(node.getLocalAddress());
                if (node.getS().isRNChildren())
                    for (int j = 0; j < node.getS().getNumChildren(); ++j)
                        rneighbours[node.getS().getSubZone(j)->getLink().getIPNum()] = true;
                else
                    for (int j = 0; j < node.getS().getNumChildren(); ++j)
                        sneighbours[node.getS().getSubZone(j)->getLink().getIPNum()].first = true;
            }
            node.fail();
        }
        // Remove the failing nodes from the neighbours list
        for (unsigned int i = 0; i < failingNodes.size(); ++i) {
            rneighbours.erase(failingNodes[i]);
            sneighbours.erase(failingNodes[i]);
        }
        // Update neighbours
        for (map<uint32_t, bool>::iterator i = rneighbours.begin(); i != rneighbours.end(); ++i) {
            sim.setCurrentNode(i->first);
            sim.getNode(i->first).getE().fireFatherChanged(i->second);
        }
        for (map<uint32_t, pair<bool, list<CommAddress> > >::iterator i = sneighbours.begin(); i != sneighbours.end(); ++i) {
            sim.setCurrentNode(i->first);
            sim.getNode(i->first).getS().fireCommitChanges(i->second.first, i->second.second);
            sim.getNode(i->first).getS().fireCommitChanges(i->second.first, i->second.second);
        }
        // Program next failure
        if (numFailures == 0 || numFailed < numFailures ) {
            unsigned int numFailing = Simulator::uniform(minFail, maxFail, 1);
            programFailure(Duration(Simulator::exponential(meanTime * numFailing)), numFailing);
        }
        return true;
    }
    return false;
}
