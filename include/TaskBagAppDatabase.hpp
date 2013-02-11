/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya, María Ángeles Giménez
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

#ifndef TASKBAGAPPDATABASE_H_
#define TASKBAGAPPDATABASE_H_

#include <string>
//#include "Database.hpp"
#include "TaskDescription.hpp"
#include "Time.hpp"
#include "CommAddress.hpp"
#include "ConfigurationManager.hpp"
#include "TaskBagMsg.hpp"
class Database;


/**
 * \brief A database of bag-of-task applications.
 *
 * This class provides access to a database of bag-of-tasks applications. It provides
 * application descriptions, instances and task and request monitoring.
 **/
class TaskBagAppDatabase {
    Database * db;

    void createTables();

public:
    TaskBagAppDatabase();

    Database & getDatabase() {
        return *db;
    }

    /**
     * Creates an application
     */
    bool createApp(const std::string & name, const TaskDescription & req);

    /**
     * Create a new application instance
     * @returns Instance ID
     */
    int64_t createAppInstance(const std::string & name, Time deadline);

    /**
     * Prepares a request for all the tasks in ready state
     */
    void requestFromReadyTasks(int64_t appId, TaskBagMsg & msg);

    /**
     * Returns the application instance id for a certain request id
     */
    int64_t getInstanceId(int64_t rid);

    /**
     * Sets the search state to all the tasks in a request and sets the timeout of the request
     */
    bool startSearch(int64_t rid, Time timeout);

    /**
     * Cancels the search for tasks that are not yet allocated in a request. They are removed from
     * that request, so the request is considered allocated.
     */
    unsigned int cancelSearch(int64_t rid);

    /**
     * Sets the state of the accepted tasks to executing, and records the execution node address
     */
    unsigned int acceptedTasks(const CommAddress & src, int64_t rid, unsigned int firstRtid, unsigned int lastRtid);

    /**
     * Checks that a task belongs to a request
     */
    bool taskInRequest(unsigned int tid, int64_t rid);

    /**
     * Sets the state of a task to FINISHED, checking source address, and returns whether it succeeded or not
     */
    bool finishedTask(const CommAddress & src, int64_t rid, unsigned int tid);

    /**
     * Sets the state of a task to FINISHED, checking source address, and returns whether it succeeded or not
     */
    bool abortedTask(const CommAddress & src, int64_t rid, unsigned int tid);

    /**
     * Marks all the tasks that where being executed by a node as READY so that they can be resent
     */
    void deadNode(const CommAddress & fail);

    /**
     * Returns the number of finished tasks of an application
     */
    unsigned long int getNumFinished(int64_t appId);

    /**
     * Returns the number of ready tasks of an application
     */
    unsigned long int getNumReady(int64_t appId);

    /**
     * Returns the number of executing tasks of an application
     */
    unsigned long int getNumExecuting(int64_t appId);

    /**
     * Returns the number of tasks of an application in execution or search state
     */
    unsigned long int getNumInProcess(int64_t appId);

    /**
     * Returns whether an application instance is finished
     */
    bool isFinished(int64_t appId);

    /**
     * Return the release time of an application instance
     */
    Time getReleaseTime(int64_t appId);
};

#endif /*TASKBAGAPPDATABASE_H_*/
