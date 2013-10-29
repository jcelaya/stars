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

#ifndef TASKMONITORMSG_H_
#define TASKMONITORMSG_H_

#include <vector>
#include <utility>
#include "BasicMsg.hpp"


class TaskMonitorMsg: public BasicMsg {
public:
    MESSAGE_SUBCLASS(TaskMonitorMsg);

    /**
     * Returns the number of tasks
     */
    unsigned int getNumTasks() const {
        return tasks.size();
    }

    /**
     * Returns the ID of the ith task contained in this request.
     * @return ID of task.
     */
    uint32_t getTaskId(unsigned int i) const {
        return tasks[i].second;
    }

    /**
     * Returns the ID of the ith task contained in this request.
     * @return ID of task.
     */
    int64_t getRequestId(unsigned int i) const {
        return tasks[i].first;
    }

    /**
     * Returns the ID of the ith task contained in this request.
     * @return ID of task.
     */
    int32_t getTaskState(unsigned int i) const {
        return states[i];
    }

    /**
     * Adds the ID of a task that is requested to be aborted.
     * @param n ID of task.
     */
    void addTask(int64_t rid, uint32_t tid, int32_t s) {
        tasks.push_back(std::make_pair(rid, tid));
        states.push_back(s);
    }

    /**
     * Returns the number of seconds that are expected until next heartbeat.
     * @return ID of task.
     */
    int32_t getHeartbeat() const {
        return heartbeat;
    }

    /**
     * Sets the number of seconds that are expected until next heartbeat.
     * @param n ID of task.
     */
    void setHeartbeat(int32_t n) {
        heartbeat = n;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE(tasks, states, heartbeat);
private:
    std::vector<std::pair<int64_t, uint32_t> > tasks;
    std::vector<int32_t> states;
    int32_t heartbeat;     ///< seconds until next monitoring report
};

#endif /* TASKMONITORMSG_H_ */
