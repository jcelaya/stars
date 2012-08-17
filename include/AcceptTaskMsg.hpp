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

#ifndef ACCEPTTASKMSG_H_
#define ACCEPTTASKMSG_H_

#include "BasicMsg.hpp"


class AcceptTaskMsg: public BasicMsg {
public:
    MESSAGE_SUBCLASS(AcceptTaskMsg);

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
     * Returns the ID of the first task contained in this request.
     * @return ID of task.
     */
    uint32_t getFirstTask() const {
        return firstTask;
    }

    /**
     * Sets the ID of the first task that is requested to be assigned.
     * @param n ID of task.
     */
    void setFirstTask(uint32_t n) {
        firstTask = n;
    }

    /**
     * Returns the ID of the last task contained in this request.
     * @return ID of task.
     */
    uint32_t getLastTask() const {
        return lastTask;
    }

    /**
     * Sets the ID of the last task that is requested to be assigned.
     * @param n ID of task.
     */
    void setLastTask(uint32_t n) {
        lastTask = n;
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

    MSGPACK_DEFINE(requestId, firstTask, lastTask, heartbeat);
private:
    int64_t requestId;    ///< Request ID relative to the requester
    uint32_t firstTask;   ///< Id of the first task in the interval to assign.
    uint32_t lastTask;    ///< Id of the last task in the interval to assign.
    int32_t heartbeat;     ///< seconds until next monitoring report
};

#endif /* ACCEPTTASKMSG_H_ */
