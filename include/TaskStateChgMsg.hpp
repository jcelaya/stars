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

#ifndef TASKSTATECHGMSG_H_
#define TASKSTATECHGMSG_H_

#include "TaskEventMsg.hpp"


/**
 * \brief A task state change notification.
 *
 * This kind of TaskEventMsg is received whenever the sending task changes its state.
 */
class TaskStateChgMsg : public TaskEventMsg {
public:
    MESSAGE_SUBCLASS(TaskStateChgMsg);

    /**
     * Returns the state the task was in before it changed.
     * @return The old state.
     */
    int32_t getOldState() const {
        return oldState;
    }

    /**
     * Sets the state the task was in before it changed.
     * @param v The old state.
     */
    void setOldState(int32_t v) {
        oldState = v;
    }

    /**
     * Returns the state the task is in after it has changed.
     * @return The new state.
     */
    int32_t getNewState() const {
        return newState;
    }

    /**
     * Sets the state the task is in after it has changed.
     * @param v The new state.
     */
    void setNewState(int32_t v) {
        newState = v;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TaskEventMsg &)*this, oldState, newState);
private:
    int32_t oldState;   ///< The old state of the task
    int32_t newState;   ///< The new state
};

#endif /*TASKSTATECHGMSG_H_*/
