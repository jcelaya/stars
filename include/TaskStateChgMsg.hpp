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

#ifndef TASKSTATECHGMSG_H_
#define TASKSTATECHGMSG_H_

#include "TaskEventMsg.hpp"


/**
 * \brief A task state change notification.
 *
 * This kind of TaskEventMsg is received whenever the sending task changes its state.
 */
class TaskStateChgMsg : public TaskEventMsg {
    /// Set the basic elements for a Serializable descendant
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(TaskEventMsg) & oldState & newState;
    }

    int32_t oldState;   ///< The old state of the task
    int32_t newState;   ///< The new state

public:
    // This is documented in BasicMsg
    virtual TaskStateChgMsg * clone() const {
        return new TaskStateChgMsg(*this);
    }

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

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("TaskStateChgMsg");
    }
};

#endif /*TASKSTATECHGMSG_H_*/
