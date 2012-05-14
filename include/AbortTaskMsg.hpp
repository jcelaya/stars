/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2010 Javier Celaya
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

#ifndef ABORTTASKMSG_H_
#define ABORTTASKMSG_H_

#include <vector>
#include "BasicMsg.hpp"


class AbortTaskMsg : public BasicMsg {
    /// Set the basic elements for a Serializable class
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(BasicMsg) & requestId & tasks;
    }

    int64_t requestId;        ///< Request ID relative to the requester
    std::vector<uint32_t> tasks;   ///< Id of the aborted tasks

public:
    // This is described in BasicMsg
    virtual AbortTaskMsg * clone() const {
        return new AbortTaskMsg(*this);
    }

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

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("AbortTaskMsg");
    }
};

#endif /* ABORTTASKMSG_H_ */
