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

#include "Logger.hpp"
#include "../SimulationCase.hpp"
#include "TaskDescription.hpp"
#include "DispatchCommandMsg.hpp"
#include "Time.hpp"
#include "../SimAppDatabase.hpp"
#include "SubmissionNode.hpp"
#include "TaskMonitorMsg.hpp"
#include "RequestTimeout.hpp"
#include "AvailabilityInformation.hpp"
#include "TaskStateChgMsg.hpp"
#include "../SimTask.hpp"
using namespace std;
using namespace boost;


class manySearches : public SimulationCase {
    unsigned int numSearches, nextSearch;
    int taskDelta, taskRepeat;
    DispatchCommandMsg dcm;
    TaskDescription minReq;
    Duration deadline;
    int workingNodes;
    bool finish;

public:
    manySearches(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const string getName() { return string("manySearches"); }

    void preStart() {
        Simulator & sim = Simulator::getInstance();
        // Before running simulation
        deadline = Duration(property("task_deadline", 3600.0));

        numSearches = property("num_searches", 1);
        LogMsg("Sim.Progress", WARN) << "Performing " << numSearches << " searches.";

        // Prepare the application
        minReq.setLength(property("task_length", 600000)); // 10 minutes for a power of 1000
        minReq.setMaxMemory(property("task_max_mem", 1024));
        minReq.setMaxDisk(property("task_max_disk", 1024));
        minReq.setInputSize(property("task_input_size", 0));
        minReq.setOutputSize(property("task_output_size", 0));
        minReq.setNumTasks(property("num_tasks", 10));
        ostringstream oss;
        oss << "manySearchesApp" << minReq.getNumTasks();
        SimAppDatabase::getCurrentDatabase().createAppDescription(oss.str(), minReq);

        taskDelta = property("task_delta", 0);
        taskRepeat = property("task_repeat", 1);

        // Send first app instance
        uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
        dcm.setAppName(oss.str());
        dcm.setDeadline(sim.getCurrentTime() + deadline);
        sim.injectMessage(client, client, shared_ptr<BasicMsg>(dcm.clone()), Duration());
        nextSearch = 2;
        taskRepeat--;
        workingNodes = 0;
        finish = false;
    }

    void beforeEvent(const Simulator::Event & ev) {
        if (typeid(*ev.msg) == typeid(DispatchCommandMsg) && Simulator::getInstance().getNode(ev.to).getSub().isIdle())
            workingNodes++;
    }

    void afterEvent(const Simulator::Event & ev) {
        Simulator & sim = Simulator::getInstance();
        if (sim.emptyEventQueue()) {
            if (nextSearch > numSearches) finish = true;
            else {
                uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
                if (!taskRepeat) {
                    minReq.setNumTasks(minReq.getNumTasks() + taskDelta);
                    ostringstream oss;
                    oss << "manySearchesApp" << minReq.getNumTasks();
                    SimAppDatabase::getCurrentDatabase().createAppDescription(oss.str(), minReq);
                    dcm.setAppName(oss.str());
                    taskRepeat = property("task_repeat", 1);
                }
                dcm.setDeadline(sim.getCurrentTime() + deadline);
                sim.injectMessage(client, client, shared_ptr<BasicMsg>(dcm.clone()), Duration());
                percent = (nextSearch++ * 100.0 / numSearches);
                taskRepeat--;
            }
        }
        else if (typeid(*ev.msg) == typeid(TaskMonitorMsg) || typeid(*ev.msg) == typeid(RequestTimeout) || ev.msg->getName() == "HeartbeatTimeout") {
            // Check if the node is idle
            if (sim.getNode(ev.to).getSub().isIdle())
                workingNodes--;
        }
        else if (typeid(*ev.msg) == typeid(TaskStateChgMsg)) {
            if (sim.getStarsStats().getExistingTasks() == 0) finish = true;
        }
    }

    bool doContinue() const {
        return !finish;
    }

    void postEnd() {
    }
};
REGISTER_SIMULATION_CASE(manySearches);
