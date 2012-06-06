/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
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
#include "MinSlownessScheduler.hpp"
#include <algorithm>
#include <cmath>


void MinSlownessScheduler::TaskProxy::sort(std::vector<TaskProxy> & curTasks, double slowness) {
    // Sort the vector, leaving the first task as is
    for (std::vector<TaskProxy>::iterator i = curTasks.begin(); i != curTasks.end(); ++i)
        i->setSlowness(slowness);
    std::sort(++curTasks.begin(), curTasks.end());
}


bool MinSlownessScheduler::TaskProxy::meetDeadlines(const std::vector<TaskProxy> & curTasks, double slowness) {
    double e = 0.0;
    for (std::vector<TaskProxy>::const_iterator i = curTasks.begin(); i != curTasks.end(); ++i)
        if ((e += i->t) > i->getDeadline(slowness))
            return false;
        return true;
}


void MinSlownessScheduler::TaskProxy::sortMinSlowness(std::vector<TaskProxy> & curTasks, const std::vector<double> & lBounds) {
    // Calculate interval by binary search
    unsigned int minLi = 0, maxLi = lBounds.size() - 1;
    while (maxLi > minLi + 1) {
        unsigned int medLi = (minLi + maxLi) >> 1;
        TaskProxy::sort(curTasks, (lBounds[medLi] + lBounds[medLi + 1]) / 2.0);
        // For each app, check whether it is going to finish in time or not.
        if (TaskProxy::meetDeadlines(curTasks, lBounds[medLi]))
            maxLi = medLi;
        else
            minLi = medLi;
    }
    // Sort them one last time
    TaskProxy::sort(curTasks, (lBounds[minLi] + lBounds[minLi + 1]) / 2.0);
}


double MinSlownessScheduler::sortMinSlowness(std::list<boost::shared_ptr<Task> > & tasks) {
    if (!tasks.empty()) {
        // List of tasks
        Time now = Time::getCurrentTime();
        std::vector<TaskProxy> tps;
        tps.reserve(tasks.size());
        for (std::list<boost::shared_ptr<Task> >::const_iterator i = tasks.begin(); i != tasks.end(); ++i)
            tps.push_back(TaskProxy(*i, now));
        
        // List of slowness values where existing tasks change order, except the first one
            std::vector<double> lBounds(1, 0.0);
            for (std::vector<TaskProxy>::iterator it = ++tps.begin(); it != tps.end(); ++it)
                for (std::vector<TaskProxy>::iterator jt = it; jt != tps.end(); ++jt)
                    if (it->a != jt->a) {
                        double l = (jt->r - it->r) / (it->a - jt->a);
                        if (l > 0.0) {
                            lBounds.push_back(l);
                        }
                    }
                    std::sort(lBounds.begin(), lBounds.end());
                lBounds.push_back(lBounds.back() + 1.0);
            TaskProxy::sortMinSlowness(tps, lBounds);
            
            // Reconstruct task list and calculate minimum slowness
            tasks.clear();
            double minSlowness = 0.0, e = 0.0;
            // For each task, calculate finishing time
            for (std::vector<TaskProxy>::iterator i = tps.begin(); i != tps.end(); ++i) {
                tasks.push_back(i->origin);
                e += i->t;
                double slowness = (e - i->r) / i->a;
                if (slowness > minSlowness)
                    minSlowness = slowness;
            }
            
            return minSlowness;
    } else return 0.0;
}


void MinSlownessScheduler::reschedule() {
    double minSlowness = sortMinSlowness(tasks);
    
    LogMsg("Ex.Sch.MS", DEBUG) << "Current minimum slowness: " << minSlowness;
    
    info.setAvailability(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(), tasks, backend.impl->getAveragePower(), minSlowness);
    
    // Start first task if it is not executing yet.
    if (!tasks.empty()) {
        if (tasks.front()->getStatus() == Task::Prepared) tasks.front()->run();
        // Program a timer
        rescheduleAt(Time::getCurrentTime() + Duration(600.0));
    }
}


unsigned int MinSlownessScheduler::accept(const TaskBagMsg & msg) {
    // Always accept new tasks
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    LogMsg("Ex.Sch.MS", INFO) << "Accepting " << numAccepted << " tasks from " << msg.getRequester();
    
    // Now create the tasks and add them to the task list
    for (unsigned int i = 0; i < numAccepted; i++)
        tasks.push_back(backend.impl->createTask(msg.getRequester(), msg.getRequestId(), msg.getFirstTask() + i, msg.getMinRequirements()));
    reschedule();
    notifySchedule();
    return numAccepted;
}
