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

#ifndef ABORTTASKMSG_H_
#define ABORTTASKMSG_H_

#include <vector>
#include "BasicMsg.hpp"


class AbortTaskMsg : public BasicMsg {
public:
    MESSAGE_SUBCLASS(AbortTaskMsg);

    /**
     * Obtains the request ID
     */
    int64_t getRequestId() const {
        return requestId;
    }

    /**
     * Sets the request ID
     */
    void setRequestId(int64_t v) {
        requestId = v;
    }

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
    uint32_t getTask(unsigned int i) const {
        return tasks[i];
    }

    /**
     * Adds the ID of a task that is requested to be aborted.
     * @param n ID of task.
     */
    void addTask(uint32_t n) {
        tasks.push_back(n);
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE(requestId, tasks);
private:
    int64_t requestId;        ///< Request ID relative to the requester
    std::vector<uint32_t> tasks;   ///< Id of the aborted tasks
};

#endif /* ABORTTASKMSG_H_ */
