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
    meanTime = median_session / (Simulator::getInstance().getNumNodes() * log(2.0));
    minFail = minf;
    maxFail = maxf;
    numFailures = 0;
    randomFailure();
}


void FailureGenerator::randomFailure() {
    LogMsg("Sim.Fail", DEBUG) << "Generating new failure";
    // Get number of failing nodes
    size_t numFailing = Simulator::uniform(minFail, maxFail, 1);
    Simulator & sim = Simulator::getInstance();
    if (numFailing > sim.getNumNodes()) numFailing = sim.getNumNodes();
    Duration failAt(Simulator::exponential(meanTime * numFailing));
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
        Simulator & sim = Simulator::getInstance();
        // nodes fail!!
        LogMsg("Sim.Fail", DEBUG) << failingNodes.size() << " nodes FAIL at " << sim.getCurrentTime();
        for (unsigned int i = 0; i < failingNodes.size(); ++i)
            sim.getNode(failingNodes[i]).fail();
        // Program next failure
        randomFailure();
        return true;
    }
    return false;
}
