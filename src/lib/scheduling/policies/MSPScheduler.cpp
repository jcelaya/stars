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
#include "MSPScheduler.hpp"
#include <algorithm>
#include <cmath>
using std::vector;
using std::list;


double MSPScheduler::sortMinSlowness(TaskProxy::List & proxys, const vector<double> & switchValues, list<boost::shared_ptr<Task> > & tasks) {
    if (!proxys.empty()) {
        proxys.sortMinSlowness(switchValues);

        // Reconstruct task list
        tasks.clear();
        for (TaskProxy::List::iterator i = proxys.begin(); i != proxys.end(); ++i) {
            tasks.push_back(i->origin);
        }

        return proxys.getSlowness();
    } else return 0.0;
}


void MSPScheduler::reschedule() {
    if (!proxys.empty()) {
        // Adjust the time of the first task
        proxys.front().t = proxys.front().origin->getEstimatedDuration().seconds();
    }

    double minSlowness = 0.0;
    if (dirty) {
        proxys.getSwitchValues(switchValues);
        dirty = false;
    }

    if (!proxys.empty()) {
        minSlowness = sortMinSlowness(proxys, switchValues, tasks);
    }

    LogMsg("Ex.Sch.MS", DEBUG) << "Minimum slowness " << minSlowness;

    info.setAvailability(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(),
            proxys, switchValues, backend.impl->getAveragePower(), minSlowness);

    // Start first task if it is not executing yet.
    if (!tasks.empty()) {
        if (tasks.front()->getStatus() == Task::Prepared) {
            tasks.front()->run();
            startedTaskEvent(*tasks.front());
        }
        // Program a timer
        rescheduleAt(Time::getCurrentTime() + Duration(600.0));
    }
}


unsigned int MSPScheduler::acceptable(const TaskBagMsg & msg) {
    // Always accept new tasks
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    LogMsg("Ex.Sch.MS", INFO) << "Accepting " << numAccepted << " tasks from " << msg.getRequester();
    return numAccepted;
}


void MSPScheduler::removeTask(const boost::shared_ptr<Task> & task) {
    // Look for the proxy
    for (TaskProxy::List::iterator p = proxys.begin(); p != proxys.end(); ++p) {
        if (p->id == task->getTaskId()) {
            // Remove the proxy
            proxys.erase(p);
            dirty = true;
            break;
        }
    }
}


void MSPScheduler::acceptTask(const boost::shared_ptr<Task> & task) {
    // Add a new proxy for this task
    proxys.push_back(TaskProxy(task));
    dirty = true;
}
