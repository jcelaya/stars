/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
 *
 *  This file is part of STaRS.
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

#ifndef SIMTASK_H_
#define SIMTASK_H_

#include "Task.hpp"
class TaskDescription;


class SimTask : public Task {
    static unsigned int runningTasks;
    int timer;
    Duration taskDuration;
    Time finishTime;

public:
    SimTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d);
    ~SimTask() {
        runningTasks--;
    }

    int getStatus() const;

    void run();

    void abort();

    Duration getEstimatedDuration() const;

    static unsigned int getRunningTasks() {
        return runningTasks;
    }
};

#endif /*SIMTASK_H_*/
