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
#include "SimpleScheduler.hpp"
#include "BasicAvailabilityInfo.hpp"
#include "TaskBagMsg.hpp"


void SimpleScheduler::reschedule() {
    if (tasks.empty()) {
        LogMsg("Ex.Sch.Simple", DEBUG) << "Simple@" << this << ": No tasks";
        info.reset();
        info.addNode(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk());
    } else {
        Time estimatedFinish = Time::getCurrentTime() + tasks.front()->getEstimatedDuration();
        LogMsg("Ex.Sch.Simple", DEBUG) << "Simple@" << this << ": One task, finishes at " << estimatedFinish;
        // If the first task is not running, start it!
        if (tasks.front()->getStatus() == Task::Prepared) tasks.front()->run();

        info.reset();
    }
}


unsigned int SimpleScheduler::acceptable(const TaskBagMsg & msg) {
    // Only accept one task
    if (tasks.empty() && msg.getLastTask() >= msg.getFirstTask()) {
        LogMsg("Ex.Sch.Simple", INFO) << "Accepting 1 task from " << msg.getRequester();
        return 1;
    } else {
        LogMsg("Ex.Sch.Simple", INFO) << "Rejecting 1 task from " << msg.getRequester();
        return 0;
    }
}
