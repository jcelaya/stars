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

#ifndef TASKEVENTMSG_H_
#define TASKEVENTMSG_H_

#include "BasicMsg.hpp"


/**
 * \brief Notification message of an occurred event related to a task.
 *
 * This kind of message is received by a Scheduler whenever a task
 * wants to notify anything; f. e. whenever a task finishes.
 */
class TaskEventMsg : public BasicMsg {
public:
    MESSAGE_SUBCLASS(TaskEventMsg);

    // Getters and Setters

    /**
     * Returns the ID of the notifying task. The IDs are local to each execution node,
     * so this kind of message is always checked for its source address being the
     * local address.
     * @return Task ID.
     */
    uint32_t getTaskId() const {
        return taskId;
    }

    /**
     * Sets the task ID.
     * @param id New task ID.
     */
    void setTaskId(uint32_t id) {
        taskId = id;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE(taskId);
private:
    uint32_t taskId;   ///< ID of the notifier task.
};

#endif /*TASKEVENTMSG_H_*/
