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

#ifndef RESCHEDULEMSG_H_
#define RESCHEDULEMSG_H_

#include <vector>
#include "TaskBagMsg.hpp"

namespace stars {

class RescheduleMsg: public TaskBagMsg {
public:
    struct TaskId {
        CommAddress requester;
        int64_t requestId;
        uint32_t taskId;
        MSGPACK_DEFINE(requester, requestId, taskId);
    };

    RescheduleMsg() {}
    RescheduleMsg(const RescheduleMsg & copy) : TaskBagMsg(copy), taskSequence(copy.taskSequence) {}
    RescheduleMsg(const TaskBagMsg & copy) : TaskBagMsg(copy) {}

    MESSAGE_SUBCLASS(RescheduleMsg);

    void setSequenceLength(unsigned int n) {
        taskSequence.reserve(n);
    }
    void addTask(CommAddress requester, int64_t requestId, uint32_t taskId) {
        taskSequence.push_back(TaskId{requester, requestId, taskId});
    }
    std::vector<TaskId> getTaskSequence() const {
        return taskSequence;
    }

    uint32_t getSeqNumber() const {
        return seq; // seq is not used with the centralized scheduler
    }

    void setSeqNumber(uint32_t s) {
        seq = s;
    }

    MSGPACK_DEFINE((TaskBagMsg &)*this, taskSequence);

private:
    std::vector<TaskId> taskSequence;
};

} /* namespace stars */

#endif /* RESCHEDULEMSG_H_ */
