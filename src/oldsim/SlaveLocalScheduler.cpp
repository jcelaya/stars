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

#include "SlaveLocalScheduler.hpp"
#include "Logger.hpp"

namespace stars {


void SlaveLocalScheduler::reschedule() {
    if (taskSequence.size() < tasks.size())
        Logger::msg("Sim.Cent", ERROR, "Less tasks in the cent queue than in node");
    std::list<std::shared_ptr<Task> > newTasks;
    for (auto i : taskSequence) {
        auto task = getTask(i.requester, i.requestId, i.taskId);
        if (task.get())
            newTasks.push_back(task);
    }
    tasks.swap(newTasks);
}


unsigned int SlaveLocalScheduler::acceptable(const TaskBagMsg & msg) {
    const RescheduleMsg & rm = static_cast<const RescheduleMsg &>(msg);
    if (rm.getSeqNumber() > seq) {
        seq = rm.getSeqNumber();
        taskSequence = std::move(rm.getTaskSequence());
    }
    return msg.getLastTask() - msg.getFirstTask() + 1;
}


std::shared_ptr<Task> SlaveLocalScheduler::getTask(const CommAddress & requester, int64_t rid, uint32_t tid) {
    // Check that the id exists
    for (auto i : tasks) {
        if (i->getOwner() == requester && i->getClientRequestId() == rid && i->getClientTaskId() == tid)
            return i;
    }
    return std::shared_ptr<Task>();
}

} /* namespace stars */
