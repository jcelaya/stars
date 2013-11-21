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
#include "SimulationCase.hpp"
#include "Simulator.hpp"
#include "TaskDescription.hpp"
#include "DispatchCommandMsg.hpp"
#include "Time.hpp"
#include "SimAppDatabase.hpp"
#include "SubmissionNode.hpp"
#include "TaskMonitorMsg.hpp"
#include "RequestTimeout.hpp"
#include "AvailabilityInformation.hpp"
#include "TaskStateChgMsg.hpp"
#include "SimTask.hpp"
#include "Variables.hpp"
using namespace std;
using namespace boost;


class manySearches : public SimulationCase {
    unsigned int numSearches, nextSearch;
    int taskDelta, taskRepeat;
    DispatchCommandMsg dcm;
    TaskDescription minReq;
    Duration deadline;
    DiscreteUniformVariable clientVar;

public:
    manySearches(const Properties & p) : SimulationCase(p), clientVar(0, 1) {
        // Prepare the properties
    }

    static const string getName() { return string("manySearches"); }

    void preStart() {
        Simulator & sim = Simulator::getInstance();
        // Before running simulation
        deadline = Duration(property("task_deadline", 3600.0));
        clientVar = DiscreteUniformVariable(0, sim.getNumNodes() - 1);

        numSearches = property("num_searches", 1);
        Logger::msg("Sim.Progress", WARN, "Performing ", numSearches, " searches.");

        // Prepare the application
        minReq.setLength(property("task_length", 600000)); // 10 minutes for a power of 1000
        minReq.setMaxMemory(property("task_max_mem", 1024));
        minReq.setMaxDisk(property("task_max_disk", 1024));
        minReq.setInputSize(property("task_input_size", 0));
        minReq.setOutputSize(property("task_output_size", 0));
        minReq.setNumTasks(property("num_tasks", 10));
        SimAppDatabase::getCurrentDatabase().setNextApp(minReq);

        taskDelta = property("task_delta", 0);
        taskRepeat = property("task_repeat", 1);

        // Send first app instance
        uint32_t client = clientVar();
        dcm.setDeadline(sim.getCurrentTime() + deadline);
        sim.injectMessage(client, client, std::shared_ptr<BasicMsg>(dcm.clone()), Duration());
        nextSearch = 2;
        taskRepeat--;
    }

    void afterEvent(const Simulator::Event & ev) {
        Simulator & sim = Simulator::getInstance();
        if (sim.emptyEventQueue() && nextSearch <= numSearches) {
            uint32_t client = clientVar();
            if (!taskRepeat) {
                minReq.setNumTasks(minReq.getNumTasks() + taskDelta);
                SimAppDatabase::getCurrentDatabase().setNextApp(minReq);
                taskRepeat = property("task_repeat", 1);
            }
            dcm.setDeadline(sim.getCurrentTime() + deadline);
            sim.injectMessage(client, client, std::shared_ptr<BasicMsg>(dcm.clone()), Duration());
            percent = (nextSearch++ * 100.0 / numSearches);
            taskRepeat--;
        }
    }

    bool doContinue() const {
        return nextSearch <= numSearches || Simulator::getInstance().getStarsStatistics().getExistingTasks() > 0;
    }

    void postEnd() {
    }
};
REGISTER_SIMULATION_CASE(manySearches);
