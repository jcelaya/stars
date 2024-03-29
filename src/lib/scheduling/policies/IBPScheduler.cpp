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
#include "IBPScheduler.hpp"
#include "IBPAvailabilityInformation.hpp"
#include "TaskBagMsg.hpp"


void IBPScheduler::reschedule() {
    if (tasks.empty()) {
        Logger::msg("Ex.Sch.Simple", DEBUG, "Simple@", this, ": No tasks");
    } else {
        Time estimatedFinish = Time::getCurrentTime() + tasks.front()->getEstimatedDuration();
        Logger::msg("Ex.Sch.Simple", DEBUG, "Simple@", this, ": One task, finishes at ", estimatedFinish);
    }
}


IBPAvailabilityInformation * IBPScheduler::getAvailability() const {
    IBPAvailabilityInformation * info = new IBPAvailabilityInformation;
    if (tasks.empty()) {
        info->addNode(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk());
    }
    return info;
}


unsigned int IBPScheduler::acceptable(const TaskBagMsg & msg) {
    // Only accept one task
    if (tasks.empty() && msg.getLastTask() >= msg.getFirstTask()) {
        Logger::msg("Ex.Sch.Simple", INFO, "Accepting 1 task from ", msg.getRequester());
        return 1;
    } else {
        Logger::msg("Ex.Sch.Simple", INFO, "Rejecting 1 task from ", msg.getRequester());
        return 0;
    }
}
