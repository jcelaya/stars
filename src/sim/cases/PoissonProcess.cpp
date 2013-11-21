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

#include <boost/thread/mutex.hpp>
#include "Logger.hpp"
#include "SimulationCase.hpp"
#include "Simulator.hpp"
#include "Time.hpp"
#include "DispatchCommandMsg.hpp"
#include "RequestGenerator.hpp"


class PoissonProcess : public SimulationCase {
    unsigned int numInstances, nextInstance, remainingApps;
    double meanTime;
    RequestGenerator rg;
    boost::mutex m;

public:
    PoissonProcess(const Properties & p) : SimulationCase(p), rg(p) {
        // Prepare the properties
    }

    static const std::string getName() {
        return std::string("poisson_process");
    }

    virtual void preStart() {
        // Before running simulation
        Simulator & sim = Simulator::getInstance();
        Time now = Time::getCurrentTime();

        // Simulation limit
        numInstances = property("num_searches", 1);
        LogMsg("Sim.Progress", 0) << "Performing " << numInstances << " searches.";

        // Global variables
        meanTime = property("mean_time", 60.0) * sim.getNumNodes();

        // Send first request to every node without exceeding the number of instances
        nextInstance = 0;
        for (uint32_t client = 0; client < sim.getNumNodes() && nextInstance < numInstances; ++client) {
            Time nextMsg = now + Duration(Simulator::exponential(meanTime));
            std::shared_ptr<DispatchCommandMsg> dcm( rg.generate(sim.getNode(client), nextMsg));
            // Send this message to the client
            sim.getNode(client).setTimer(nextMsg, dcm);
            nextInstance++;
        }
        remainingApps = 0;
    }

    virtual void afterEvent(CommAddress src, CommAddress dst, std::shared_ptr<BasicMsg> msg) {
        if (typeid(*msg) == typeid(DispatchCommandMsg) && nextInstance < numInstances) {
            // Calculate next send
            Time nextMsg = Time::getCurrentTime() + Duration(Simulator::exponential(meanTime));
            StarsNode & node = Simulator::getInstance().getCurrentNode();
            std::shared_ptr<DispatchCommandMsg> dcm(rg.generate(node, nextMsg));
            // Send this message to the client
            node.setTimer(nextMsg, dcm);
            nextInstance++;
        }
    }

    virtual void finishedApp(long int appId) {
        boost::mutex::scoped_lock lock(m);
        percent = (remainingApps++) * 100.0 / numInstances;
        if (remainingApps >= numInstances)
            Simulator::getInstance().stop();
    }
};
REGISTER_SIMULATION_CASE(PoissonProcess);
