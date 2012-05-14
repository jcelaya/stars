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
#include "FCFSScheduler.hpp"
#include "TaskBagMsg.hpp"
using namespace std;


/**
 * Compares two tasks by its creation time.
 * @param l Left operand task.
 * @param r Right operand task.
 * @return True if l was created earlier than r, false otherwise.
 */
static bool compareCreation(boost::shared_ptr<Task> l, boost::shared_ptr<Task> r) {
    return l->getCreationTime() < r->getCreationTime();
}


void FCFSScheduler::reschedule() {
    Time estimatedFinish = Time::getCurrentTime();
    LogMsg("Ex.Sch.FCFS", DEBUG) << "FCFS@" << this << ": Rescheduling, now is " << estimatedFinish;
    info.reset();

    if (!tasks.empty()) {
        // Order the tasks by creation time
        tasks.sort(compareCreation);

        // Calculate queue length
        for (list<boost::shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); i++) {
            // Increment the estimatedStart value
            estimatedFinish += (*i)->getEstimatedDuration();
        }
        LogMsg("Ex.Sch.FCFS", DEBUG) << "FCFS@" << this << ": Queue finishes at " << estimatedFinish;

        // If the first task is not running, start it!
        if (tasks.front()->getStatus() == Task::Prepared) tasks.front()->run();
    }

    info.addQueueEnd(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(),
                     backend.impl->getAveragePower(), estimatedFinish);
    LogMsg("Ex.Sch.FCFS", DEBUG) << "FCFS@" << this << ": Resulting info is " << info;
}


unsigned int FCFSScheduler::accept(const TaskBagMsg & msg) {
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    LogMsg("Ex.Sch.FCFS", INFO) << "Accepting " << numAccepted << " tasks from " << msg.getRequester();
    if (numAccepted == 0) return 0;

    // Now create the tasks and add them to the list
    for (unsigned int i = 0; i < numAccepted; i++)
        tasks.push_back(backend.impl->createTask(msg.getRequester(), msg.getRequestId(), msg.getFirstTask() + i, msg.getMinRequirements()));
    reschedule();
    notifySchedule();
    return numAccepted;
}
