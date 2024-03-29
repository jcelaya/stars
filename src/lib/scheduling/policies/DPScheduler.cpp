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
#include "DPScheduler.hpp"
#include "TaskBagMsg.hpp"
#include "TaskMonitorMsg.hpp"
#include "ConfigurationManager.hpp"
using namespace std;


/**
 * Compares two tasks by its deadline. However, a running task goes always before,
 * because a running task cannot be preempted.
 * @param l Left operand task.
 * @param r Right operand task.
 * @return True if l has an earlier deadline than r, or it is running; false otherwise.
 */
static bool compareDeadline(std::shared_ptr<Task> l, std::shared_ptr<Task> r) {
    return l->getStatus() == Task::Running ||
           (r->getStatus() != Task::Running && l->getDescription().getDeadline() < r->getDescription().getDeadline());
}


void DPScheduler::reschedule() {
    // Order the task by deadline
    tasks.sort(compareDeadline);

    /* TODO: Not needed right now...
    Time estimatedStart = Time::getCurrentTime();
    // For each task, check whether it is going to finish in time or not.
    for (list<shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); i++) {
     if (((*i)->getDescription().getDeadline() - estimatedStart) < (*i)->getEstimatedDuration()) {
      Logger::msg("Ex.Sch.EDF", DEBUG, "Task ", (*i)->getTaskId(), " aborted because it does not finish on time.");
      // If not, delete from the list
      TaskMonitorMsg tmm;
      tmm.addTask((*i)->getClientRequestId(), (*i)->getClientTaskId(), Task::Aborted);
      tmm.setHeartbeat(ConfigurationManager::getInstance().getHeartbeat());
      CommLayer::getInstance().sendMessage((*i)->getOwner(), tmm);
      i = tasks.erase(i);
     } else {
      // Increment the estimatedStart value
      estimatedStart += (*i)->getEstimatedDuration();
     }
    }
    */

    // Program a timer
    if (!tasks.empty()) {
        rescheduleAt(Time::getCurrentTime() + Duration(ConfigurationManager::getInstance().getRescheduleTimeout()));
    }
}


DPAvailabilityInformation * DPScheduler::getAvailability() const {
    DPAvailabilityInformation * info = new DPAvailabilityInformation;
    info->addNode(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(),
                 backend.impl->getAveragePower(), tasks);
    Logger::msg("Ex.Sch.EDF", DEBUG, "Function is ", *info);
    return info;
}


unsigned long int DPScheduler::getAvailabilityBefore(Time d) {
    Time estimatedStart = Time::getCurrentTime(), estimatedEnd = d;
    if (!tasks.empty()) {
        // First task is not preemptible
        estimatedStart += tasks.front()->getEstimatedDuration();
        list<std::shared_ptr<Task> >::iterator t = ++tasks.begin();
        for (; t != tasks.end() && (*t)->getDescription().getDeadline() <= d; t++)
            estimatedStart += (*t)->getEstimatedDuration();
        if (t != tasks.end()) {
            Time limit = tasks.back()->getDescription().getDeadline();
            list<std::shared_ptr<Task> >::reverse_iterator firstTask = --tasks.rend();
            for (list<std::shared_ptr<Task> >::reverse_iterator i = tasks.rbegin(); i != firstTask && (*i)->getDescription().getDeadline() > d; i++) {
                if (limit > (*i)->getDescription().getDeadline())
                    limit = (*i)->getDescription().getDeadline();
                limit -= (*i)->getEstimatedDuration();
            }
            if (limit < estimatedEnd) estimatedEnd = limit;
        }
    }
    if (estimatedEnd < estimatedStart) return 0;
    else return backend.impl->getAveragePower() * (estimatedEnd - estimatedStart).seconds();
}


unsigned int DPScheduler::acceptable(const TaskBagMsg & msg) {
    // Adjust the availability function to the current time
    //reschedule();
    // Check dynamic constraints
    unsigned int available = getAvailabilityBefore(msg.getMinRequirements().getDeadline());
    unsigned int numSlots = available / msg.getMinRequirements().getLength();
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    if (numSlots < numAccepted) {
        Logger::msg("Ex.Sch.EDF", INFO, "Rejecting ", (numAccepted - numSlots), " tasks from ", msg.getRequester(), ", reason:");
        Logger::msg("Ex.Sch.EDF", DEBUG, "Deadline: ", msg.getMinRequirements().getDeadline(),
                "   Lenght: ", msg.getMinRequirements().getLength(),
                " (", Duration(msg.getMinRequirements().getLength() / backend.impl->getAveragePower()), ')');
        Logger::msg("Ex.Sch.EDF", DEBUG, "Available: ", available, ", ", numSlots, " slots");
        Logger::msg("Ex.Sch.EDF", DEBUG, "Task queue:");
        for (auto & t : tasks)
            Logger::msg("Ex.Sch.EDF", DEBUG, "   ", t->getEstimatedDuration(), " l", t->getDescription().getLength(), " d", t->getDescription().getDeadline());
        unsigned int available = getAvailabilityBefore(msg.getMinRequirements().getDeadline());
        numAccepted = numSlots;
    }
    Logger::msg("Ex.Sch.EDF", INFO, "Accepting ", numAccepted, " tasks from ", msg.getRequester());
    return numAccepted;
}
