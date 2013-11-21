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
#include "MMPScheduler.hpp"
#include "TaskBagMsg.hpp"
using namespace std;


/**
 * Compares two tasks by its creation time.
 * @param l Left operand task.
 * @param r Right operand task.
 * @return True if l was created earlier than r, false otherwise.
 */
static bool compareCreation(std::shared_ptr<Task> l, std::shared_ptr<Task> r) {
    return l->getCreationTime() < r->getCreationTime();
}


void MMPScheduler::reschedule() {
    Logger::msg("Ex.Sch.FCFS", DEBUG, "FCFS@", this, ": Rescheduling");
    if (!tasks.empty()) {
        // Order the tasks by creation time
        tasks.sort(compareCreation);
    }
}


MMPAvailabilityInformation * MMPScheduler::getAvailability() const {
    MMPAvailabilityInformation * info = new MMPAvailabilityInformation;
    Time estimatedFinish = Time::getCurrentTime();
    // Calculate queue length
    for (auto i = tasks.begin(); i != tasks.end(); i++) {
        estimatedFinish += (*i)->getEstimatedDuration();
    }
    Logger::msg("Ex.Sch.FCFS", DEBUG, "FCFS@", this, ": Queue finishes at ", estimatedFinish);
    info->setQueueEnd(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(),
                      backend.impl->getAveragePower(), estimatedFinish);
    info->setMaxQueueLength(estimatedFinish);
    Logger::msg("Ex.Sch.FCFS", DEBUG, "FCFS@", this, ": Resulting info is ", *info);
    return info;
}


unsigned int MMPScheduler::acceptable(const TaskBagMsg & msg) {
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    Logger::msg("Ex.Sch.FCFS", INFO, "Accepting ", numAccepted, " tasks from ", msg.getRequester());
    return numAccepted;
}
