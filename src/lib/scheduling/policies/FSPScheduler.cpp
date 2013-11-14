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
#include "FSPScheduler.hpp"
#include "ConfigurationManager.hpp"


namespace stars {

void FSPScheduler::reschedule() {
    if (!proxys.empty()) {
        // Adjust the time of the first task
        proxys.front().t = proxys.front().origin->getEstimatedDuration().seconds();
    }

    proxys.sortMinSlowness();
    // Reconstruct task list
    tasks.clear();
    for (auto & i: proxys) {
        tasks.push_back(i.origin);
    }
    Logger::msg("Ex.Sch.MS", DEBUG, "Minimum slowness ", proxys.getSlowness());

    // Start first task if it is not executing yet.
    if (!tasks.empty()) {
        for (auto it = ++tasks.begin(); it != tasks.end(); ++it)
            (*it)->pause();

        if (tasks.front()->getStatus() == Task::Prepared) {
            tasks.front()->run();
            startedTaskEvent(*tasks.front());
        }
        // Program a timer
        rescheduleAt(Time::getCurrentTime() + Duration(ConfigurationManager::getInstance().getRescheduleTimeout()));
    }
}


FSPAvailabilityInformation * FSPScheduler::getAvailability() const {
    FSPAvailabilityInformation * info = new FSPAvailabilityInformation;
    info->setAvailability(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(),
            proxys, backend.impl->getAveragePower());
    return info;
}


unsigned int FSPScheduler::acceptable(const TaskBagMsg & msg) {
    // Always accept new tasks
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    Logger::msg("Ex.Sch.MS", INFO, "Accepting ", numAccepted, " tasks from ", msg.getRequester());
    return numAccepted;
}

} // namespace stars
