/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2008 María Ángeles Giménez, Javier Celaya
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

#ifndef TASKBAGAPPDATABASE_H_
#define TASKBAGAPPDATABASE_H_

#include <string>
#include "Database.hpp"
#include "TaskDescription.hpp"
#include "Time.hpp"
#include "CommAddress.hpp"
#include "ConfigurationManager.hpp"
#include "TaskBagMsg.hpp"


/**
 * \brief A database of bag-of-task applications.
 *
 * This class provides access to a database of bag-of-tasks applications. It provides
 * application descriptions, instances and task and request monitoring.
 **/
class TaskBagAppDatabase {
	Database db;

	void createTables();

public:
	TaskBagAppDatabase();

	Database & getDatabase() { return db; }

	/**
	 * Creates an application
	 */
	void createApp(const std::string & name, const TaskDescription & req);

	/**
	 * Retrieve the application requirements
	 */
	void getAppRequirements(long int appId, TaskDescription & req);

	/**
	 * Create a new application instance
	 * @returns Instance ID
	 */
	long int createAppInstance(const std::string & name, Time deadline);

	/**
	 * Prepares a request for all the tasks in ready state
	 */
	void requestFromReadyTasks(long int appId, TaskBagMsg & msg);

	/**
	 * Returns the application instance id for a certain request id
	 */
	long int getInstanceId(long int rid);

	/**
	 * Sets the search state to all the tasks in a request and sets the timeout of the request
	 */
	void startSearch(long int rid, Time timeout);

	/**
	 * Cancels the search for tasks that are not yet allocated in a request. They are removed from
	 * that request, so the request is considered allocated.
	 */
	unsigned int cancelSearch(long int rid);

	/**
	 * Sets the state of the accepted tasks to executing, and records the execution node address
	 */
	void acceptedTasks(const CommAddress & src, long int rid, unsigned int firstRtid, unsigned int lastRtid);

	/**
	 * Checks that a task belongs to a request
	 */
	bool taskInRequest(unsigned int tid, long int rid);

	unsigned int getNumTasksInNode(const CommAddress & node);

	void getAppsInNode(const CommAddress & node, std::list<long int> & apps);

	/**
	 * Sets the state of a task to FINISHED, checking source address, and returns whether it succeeded or not
	 */
	bool finishedTask(const CommAddress & src, long int rid, unsigned int tid);

	/**
	 * Sets the state of a task to FINISHED, checking source address, and returns whether it succeeded or not
	 */
	bool abortedTask(const CommAddress & src, long int rid, unsigned int tid);

	/**
	 * Marks all the tasks that where being executed by a node as READY so that they can be resent
	 */
	void deadNode(const CommAddress & fail);

	/**
	 * Returns the number of finished tasks of an application
	 */
	unsigned long int getNumFinished(long int appId);

	/**
	 * Returns the number of ready tasks of an application
	 */
	unsigned long int getNumReady(long int appId);

	/**
	 * Returns the number of executing tasks of an application
	 */
	unsigned long int getNumExecuting(long int appId);

	/**
	 * Returns the number of tasks of an application in execution or search state
	 */
	unsigned long int getNumInProcess(long int appId);

	/**
	 * Returns whether an application instance is finished
	 */
	bool isFinished(long int appId);

	/**
	 * Return the release time of an application instance
	 */
	Time getReleaseTime(long int appId);
};

#endif /*TASKBAGAPPDATABASE_H_*/
