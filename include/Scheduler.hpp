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

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <list>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include "Time.hpp"
#include "CommLayer.hpp"
#include "TaskBagMsg.hpp"
#include "Task.hpp"
#include "AvailabilityInformation.hpp"
#include "ResourceNode.hpp"

/**
 * \brief Scheduler object interface.
 *
 * This abstract class describes the scheduler object. It contains an ordered list of
 * tasks, and is responsible for preparing and executing them in the best order. The
 * particular ordering algorithm is implemented by each subclass.
 */
class Scheduler : public Service, public ResourceNodeObserver {
public:
    /**
     * Interface for the execution environment. The scheduler can obtain information from it
     * in order to execute tasks.
     */
    class ExecutionEnvironment {
    public:
        virtual ~ExecutionEnvironment() {}

        /**
         * Returns the average computing power of this node, with the offline time taken into account.
         * TODO: Count number of processors.
         * @return Computing power.
         */
        virtual double getAveragePower() const = 0;

        /**
         * Returns the available memory for task execution. It should be constant during the use of
         * Peercomp.
         * @return Available memory.
         */
        virtual unsigned long int getAvailableMemory() const = 0;

        /**
         * Returns the available disk space for input, output and temporary files.
         * @return Available disk space.
         */
        virtual unsigned long int getAvailableDisk() const = 0;

        /**
         * Creates an implementation dependent Task object.
         * On failure, the new task starts with state Aborted.
         * @param o The address of the task owner node.
         * @param reqId The ID of the request which this task arrived in.
         * @param ctId The ID of this task relative to its request.
         * @param d TaskDescription with the task requirements.
         */
        virtual boost::shared_ptr<Task> createTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d) const = 0;
    };

    Scheduler(ResourceNode & rn) : ResourceNodeObserver(rn), seqNum(0),
            inChange(false), dirty(false), rescheduleTimer(0), monitorTimer(0), tasksExecuted(0) {}

    /**
     * Sets the relation with the ExecutionNode.
     * @param e Related ExecutionNode.
     */
    virtual ~Scheduler() {}

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    /**
     * Tries to accept a number of tasks. If so, they are added to the list.
     * @return The number of tasks really accepted.
     */
    virtual unsigned int acceptable(const TaskBagMsg & msg) = 0;

    unsigned int accept(const TaskBagMsg & msg) {
        unsigned int numAccepted = acceptable(msg);
        // Now create the tasks and add them to the list
        for (unsigned int i = 0; i < numAccepted; i++) {
            tasks.push_back(backend.impl->createTask(msg.getRequester(), msg.getRequestId(), msg.getFirstTask() + i, msg.getMinRequirements()));
            acceptTask(tasks.back());
        }
        if (numAccepted) reschedule();
        return numAccepted;
    }

    /**
     * Returns the task queue.
     */
    std::list<boost::shared_ptr<Task> > & getTasks() {
        return tasks;
    }

    /**
     * Returns the task with a certain ID.
     */
    boost::shared_ptr<Task> getTask(unsigned int id);

    virtual const AvailabilityInformation & getAvailability() const = 0;

    unsigned long int getExecutedTasks() const {
        return tasksExecuted;
    }

protected:
    class ExecutionEnvironmentImpl {
    public:
        ExecutionEnvironmentImpl();
        boost::scoped_ptr<ExecutionEnvironment> impl;
    };

    std::list<boost::shared_ptr<Task> > tasks;             ///< The list of tasks.
    uint32_t seqNum;                   ///< Sequence number for the AvailabilityInformation message
    /// Hidden implementation of the execution environment
    ExecutionEnvironmentImpl backend;

    /**
     * Programs a new reschedule timer, canceling the previous one.
     * @param r The new reschedule time.
     */
    void rescheduleAt(Time r);

    void setMonitorTimer();

    /**
     * Template method to handle an arriving msg
     */
    template<class M> void handle(const CommAddress & src, const M & msg);

    /**
     * Forces a reschedule of the list of tasks. That means:
     * <ul>
     *     <li>Remove the tasks that won't meet its deadline requirements.</li>
     *     <li>Start executing the first task if there is no task running.</li>
     *     <li>Update the AvailabilityFunction and send it to the tree if it changes.</li>
     * </ul>
     */
    virtual void reschedule() = 0;

    /**
     * Notifies the ExecutionNode about the current schedule.
     */
    void notifySchedule();

    virtual void removeTask(const boost::shared_ptr<Task> & task) {}

    virtual void acceptTask(const boost::shared_ptr<Task> & task) {}

private:
    bool inChange;         ///< States whether the father of the ResourceNode is changing.
    bool dirty;            ///< States whether a change must be notified to the father
    int rescheduleTimer;   ///< Timer to program a reschedule
    int monitorTimer;      ///< Timer to send monitoring info

    // Statistics
    void queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd);
    unsigned long int tasksExecuted;   ///< Number of executed tasks since the peer started
    Duration timeRunning;              ///< Amount of time not idle

    // This is documented in ResourceNodeObserver
    void fatherChanging() {
        inChange = true;
    }

    // This is documented in ResourceNodeObserver
    void fatherChanged(bool changed) {
        inChange = false;
        if (changed) {
            seqNum = 0;
            dirty = true;
        }
        if (dirty)
            notifySchedule();
    }
};

#endif /*SCHEDULER_H_*/
