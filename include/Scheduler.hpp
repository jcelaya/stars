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
#include "Time.hpp"
#include "CommLayer.hpp"
#include "TaskBagMsg.hpp"
#include "Task.hpp"
#include "AvailabilityInformation.hpp"
#include "OverlayLeaf.hpp"

/**
 * \brief Scheduler object interface.
 *
 * This abstract class describes the scheduler object. It contains an ordered list of
 * tasks, and is responsible for preparing and executing them in the best order. The
 * particular ordering algorithm is implemented by each subclass.
 */
class Scheduler : public Service, public OverlayLeafObserver {
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
        virtual std::shared_ptr<Task> createTask(CommAddress o,
                int64_t reqId, unsigned int ctid, const TaskDescription & d) const = 0;
    };

    Scheduler(OverlayLeaf & l) : leaf(l), seqNum(0), currentTbm(NULL), inChange(false), dirty(false),
            rescheduleTimer(0), monitorTimer(0), tasksExecuted(0), maxQueueLength(0), maxPausedTasks(0) {
        leaf.registerObserver(this);
    }

    /**
     * Sets the relation with the ExecutionNode.
     * @param e Related ExecutionNode.
     */
    virtual ~Scheduler() {
        // Abort tasks
        if (!tasks.empty()) {
            tasks.front()->abort();
            tasks.clear();
        }
    }

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    /**
     * Tries to accept a number of tasks. If so, they are added to the list.
     * @return The number of tasks really accepted.
     */
    virtual unsigned int acceptable(const TaskBagMsg & msg) = 0;

    unsigned int accept(const TaskBagMsg & msg) {
        currentTbm = &msg;
        unsigned int numAccepted = acceptable(msg);
        // Now create the tasks and add them to the list
        for (unsigned int i = 0; i < numAccepted; i++) {
            tasks.push_back(backend.impl->createTask(
                    msg.getRequester(), msg.getRequestId(), msg.getFirstTask() + i, msg.getMinRequirements()));
            acceptTask(tasks.back());
        }
        if (tasks.size() > maxQueueLength) {
            maxQueueLength = tasks.size();
        }
        if (numAccepted) {
            switchContext();
        }
        currentTbm = NULL;
        return numAccepted;
    }

    /**
     * Returns the task queue.
     */
    std::list<std::shared_ptr<Task> > & getTasks() {
        return tasks;
    }

    /**
     * Returns the task with a certain ID.
     */
    std::shared_ptr<Task> getTask(unsigned int id);

    virtual AvailabilityInformation * getAvailability() const = 0;

    unsigned long int getExecutedTasks() const {
        return tasksExecuted;
    }

    unsigned int getMaxQueueLength() const {
        return maxQueueLength;
    }

    unsigned int getMaxPausedTasks() const {
        return maxPausedTasks;
    }

protected:
    class ExecutionEnvironmentImpl {
    public:
        ExecutionEnvironmentImpl();
        std::unique_ptr<ExecutionEnvironment> impl;
    };

    OverlayLeaf & leaf;
    std::list<std::shared_ptr<Task> > tasks;             ///< The list of tasks.
    uint32_t seqNum;                   ///< Sequence number for the AvailabilityInformation message
    /// Hidden implementation of the execution environment
    ExecutionEnvironmentImpl backend;
    const TaskBagMsg * currentTbm;

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
     * </ul>
     */
    virtual void reschedule() = 0;

    void switchContext() {
        reschedule();
        if (!tasks.empty()) {
            for (auto it = ++tasks.begin(); it != tasks.end(); ++it)
                (*it)->pause();

            if (tasks.front()->getStatus() == Task::Prepared) {
                tasks.front()->run();
                startedTaskEvent(*tasks.front());
            }
        }
        countPausedTasks();
    }

    /**
     * Notifies the ExecutionNode about the current schedule.
     */
    void notifySchedule();

    virtual void removeTask(const std::shared_ptr<Task> & task) {}

    virtual void acceptTask(const std::shared_ptr<Task> & task) {}

    // Statistics
    void addedTasksEvent(const TaskBagMsg & msg, unsigned int numAccepted);
    void startedTaskEvent(const Task & t);
    void finishedTaskEvent(const Task & t, int oldState, int newState);
    void countPausedTasks() {
        unsigned int numPaused = 0;
        for (auto & task : tasks) {
            if (task->isPaused())
                ++numPaused;
        }
        if (maxPausedTasks < numPaused)
            maxPausedTasks = numPaused;
    }

private:
    bool inChange;         ///< States whether the father of the ResourceNode is changing.
    bool dirty;            ///< States whether a change must be notified to the father
    int rescheduleTimer;   ///< Timer to program a reschedule
    int monitorTimer;      ///< Timer to send monitoring info

    // Statistics
    unsigned long int tasksExecuted;   ///< Number of executed tasks since the peer started
    Duration timeRunning;              ///< Amount of time not idle
    unsigned int maxQueueLength;
    unsigned int maxPausedTasks;

    // This is documented in OverlayLeafObserver
    void fatherChanging() {
        inChange = true;
    }

    // This is documented in OverlayLeafObserver
    void fatherChanged(bool changed) {
        inChange = false;
        if (changed) {
            seqNum = 0;
            dirty = true;
        }
        if (dirty)
            notifySchedule();
    }

    bool checkStaticRequirements(const TaskDescription & req);

    std::shared_ptr<Task> removeFromQueue(unsigned int id);
};

#endif /*SCHEDULER_H_*/
