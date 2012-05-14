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

#ifndef TASK_H_
#define TASK_H_

#include "Time.hpp"
#include "CommAddress.hpp"
#include "TaskDescription.hpp"


/**
 * \brief Executable task.
 *
 * This class is the base class for an executable task, created by the Scheduler object. The
 * platform dependent part is provided through specialization.
 */
class Task {
public:
    static const int Inactive = 0;   ///< Inactive state, when the task has just been created.
    static const int Prepared = 1;   ///< Prepared state, when the data has been received.
    static const int Running = 2;    ///< Running state.
    static const int Finished = 3;   ///< Finished state, before destruction.
    static const int Aborted = 4;    ///< Like finished, but with an error.

    virtual ~Task() {}

    /**
     * Returns the current status of this task.
     * @return Status.
     */
    virtual int getStatus() const = 0;

    /**
     * Starts running this task.
     */
    virtual void run() = 0;

    /**
     * Aborts the execution of a task.
     */
    virtual void abort() = 0;

    // Getters

    /**
     * Returns the ID of this task. The IDs are local to each execution node.
     * @return Task ID.
     */
    unsigned int getTaskId() const {
        return taskId;
    }

    /**
     * Returns the address of the owner node.
     * @return Address of owner node.
     */
    const CommAddress & getOwner() const {
        return owner;
    }

    /**
     * Returns the ID of this task relative to the request that the client sent.
     * @return Task ID relative to the client.
     */
    unsigned int getClientTaskId() const {
        return clientTaskId;
    }

    /**
     * Returns the ID of the request which this task came in.
     * @return Request ID relative to the client.
     */
    long int getClientRequestId() const {
        return clientRequestId;
    }

    /**
     * Returns a reference to the TaskDescription object associated with this task.
     * @return TaskDescription associated with this tsk.
     */
    const TaskDescription & getDescription() const {
        return description;
    }

    TaskDescription & getDescription() {
        return description;
    }

    /**
     * Returns the creation time of this task.
     * @return Creation time.
     */
    Time getCreationTime() const {
        return creationTime;
    }

    /**
     * Returns the estimated duration of this task. It must take into account only
     * the remaining part of this task.
     * @return Estimated duration.
     */
    virtual Duration getEstimatedDuration() const = 0;

protected:
    /**
     * Creates a Task object.
     * @param o The address of the task owner node.
     * @param reqId The ID of the request which this task arrived in.
     * @param ctId The ID of this task relative to its request.
     * @param d TaskDescription with the task requirements.
     * @throws runtime_error When the task cannot be created.
     */
    Task(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d) :
            taskId(nextId++), owner(o), clientRequestId(reqId), clientTaskId(ctid), description(d),
            creationTime(Time::getCurrentTime()) {}

    unsigned int taskId;           ///< Task ID relative to the Scheduler.
    static unsigned int nextId;    ///< ID counter
    CommAddress owner;             ///< Owner node address
    long int clientRequestId;      ///< Request ID relative to the client node.
    unsigned int clientTaskId;     ///< Task ID relative to the client node.
    TaskDescription description;   ///< Task description.
    Time creationTime;             ///< Creation time.
};

#endif /*TASK_H_*/
