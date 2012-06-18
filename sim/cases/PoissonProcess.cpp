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

#include <boost/thread/mutex.hpp>
#include "Logger.hpp"
#include "SimulationCase.hpp"
#include "Simulator.hpp"
#include "Time.hpp"
#include "DispatchCommandMsg.hpp"
#include "RequestGenerator.hpp"


class PoissonProcess : public SimulationCase {
    unsigned int numInstances, remainingApps;
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
        
        // Send first request to every node
        for (uint32_t client = 0; client < sim.getNumNodes(); ++client) {
            Time nextMsg = now + Duration(Simulator::exponential(meanTime));
            DispatchCommandMsg * dcm = rg.generate(sim.getNode(client), nextMsg);
            // Send this message to the client
            sim.getNode(client).setTimer(nextMsg, dcm);
        }
        remainingApps = 0;
    }
    
    virtual void afterEvent(CommAddress src, CommAddress dst, boost::shared_ptr<BasicMsg> msg) {
        if (typeid(*msg) == typeid(DispatchCommandMsg)) {
            // Calculate next send
            Time nextMsg = Time::getCurrentTime() + Duration(Simulator::exponential(meanTime));
            StarsNode & node = Simulator::getInstance().getCurrentNode();
            DispatchCommandMsg * dcm = rg.generate(node, nextMsg);
            // Send this message to the client
            node.setTimer(nextMsg, dcm);
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
