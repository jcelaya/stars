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

#include "Logger.hpp"
#include "EDFScheduler.hpp"
#include "TaskBagMsg.hpp"
#include "TaskMonitorMsg.hpp"
#include "ConfigurationManager.hpp"
using namespace std;


/**
 * Compares two tasks by its deadline. However, a running task goes always before,
 * because a running task cannot be preempted.
 * @param l Left operand task.
 * @param r Right operand task.
 * @return True if l has an earlier deadline than r, or it is running; false otherwise.
 */
static bool compareDeadline(boost::shared_ptr<Task> l, boost::shared_ptr<Task> r) {
	return l->getStatus() == Task::Running ||
		(r->getStatus() != Task::Running && l->getDescription().getDeadline() < r->getDescription().getDeadline());
}


void EDFScheduler::reschedule() {
	// Order the task by deadline
	tasks.sort(compareDeadline);

	/* TODO: Not needed right now...
	Time estimatedStart = Time::getCurrentTime();
	// For each task, check whether it is going to finish in time or not.
	for (list<shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); i++) {
		if (((*i)->getDescription().getDeadline() - estimatedStart) < (*i)->getEstimatedDuration()) {
			LogMsg("Ex.Sch.EDF", DEBUG) << "Task " << (*i)->getTaskId() << " aborted because it does not finish on time.";
			// If not, delete from the list
			TaskMonitorMsg tmm;
			tmm.addTask((*i)->getClientRequestId(), (*i)->getClientTaskId(), Task::Aborted);
			tmm.setHeartbeat(ConfigurationManager::getInstance().getHeartbeat());
			CommLayer::getInstance().sendMessage((*i)->getOwner(), tmm);
			i = tasks.erase(i);
		} else {
			// Increment the estimatedStart value
			estimatedStart += (*i)->getEstimatedDuration();
		}
	}
	*/

	// If the first task is not running, start it!
	if (!tasks.empty() && tasks.front()->getStatus() == Task::Prepared) tasks.front()->run();

	calculateAvailability();

	// Program a timer
	if (!tasks.empty())
		rescheduleAt(Time::getCurrentTime() + Duration(600.0));
}


void EDFScheduler::calculateAvailability() {
	list<Time> points;
	Time now = Time::getCurrentTime();
	Time horizon = now + Duration(3600.0);

	if (tasks.size() == 1) {
		points.push_back(now + tasks.front()->getEstimatedDuration());
		points.push_back(horizon);
	} else if (tasks.size() > 1) {
		Time nextStart = tasks.back()->getDescription().getDeadline() - tasks.back()->getEstimatedDuration();
		if (tasks.back()->getDescription().getDeadline() < horizon) {
			points.push_front(horizon);
			points.push_front(tasks.back()->getDescription().getDeadline());
		}
		// Calculate the estimated ending time for each scheduled task
		for (list<boost::shared_ptr<Task> >::reverse_iterator i = ++tasks.rbegin(); (*i)->getTaskId() != tasks.front()->getTaskId(); i++) {
			// If there is time between tasks, add a new hole
			if ((*i)->getDescription().getDeadline() < nextStart) {
				points.push_front(nextStart);
				points.push_front((*i)->getDescription().getDeadline());
				nextStart = (*i)->getDescription().getDeadline() - (*i)->getEstimatedDuration();
			} else {
				nextStart = nextStart - (*i)->getEstimatedDuration();
			}
		}
		// First task is special, as it is not preemptible
		points.push_front(nextStart);
		points.push_front(now + tasks.front()->getEstimatedDuration());
	}

	info.reset();
	info.addNode(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(),
			backend.impl->getAveragePower(), points);
	LogMsg("Ex.Sch.EDF", DEBUG) << "Function is " << info;
}


unsigned long int EDFScheduler::getAvailabilityBefore(Time d) {
	Time estimatedStart = Time::getCurrentTime(), estimatedEnd = d;
	if (!tasks.empty()) {
		// First task is not preemptible
		estimatedStart += tasks.front()->getEstimatedDuration();
		list<boost::shared_ptr<Task> >::iterator t = ++tasks.begin();
		for (; t != tasks.end() && (*t)->getDescription().getDeadline() <= d; t++)
			estimatedStart += (*t)->getEstimatedDuration();
		if (t != tasks.end()) {
			Time limit = tasks.back()->getDescription().getDeadline();
			for (list<boost::shared_ptr<Task> >::reverse_iterator i = tasks.rbegin(); i != tasks.rend() && (*i)->getDescription().getDeadline() > d; i++) {
				if (limit > (*i)->getDescription().getDeadline())
					limit = (*i)->getDescription().getDeadline();
				limit -= (*i)->getEstimatedDuration();
			}
			if (limit < estimatedEnd) estimatedEnd = limit;
		}
	}
	if (estimatedEnd < estimatedStart) return 0;
	else return backend.impl->getAveragePower() * (estimatedEnd - estimatedStart).seconds();
}


unsigned int EDFScheduler::accept(const TaskBagMsg & msg) {
	// Adjust the availability function to the current time
	//reschedule();
	// Check dynamic constraints
	unsigned int available = getAvailabilityBefore(msg.getMinRequirements().getDeadline());
	unsigned int numSlots = available / msg.getMinRequirements().getLength();
	unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
	if (numSlots < numAccepted) {
		LogMsg("Ex.Sch.EDF", INFO) << "Rejecting " << (numAccepted - numSlots) << " tasks from " << msg.getRequester();
		numAccepted = numSlots;
	}
	LogMsg("Ex.Sch.EDF", INFO) << "Accepting " << numAccepted << " tasks from " << msg.getRequester();
	if (numAccepted == 0) return 0;

	// Now create the tasks and add them to the list
	for (unsigned int i = 0; i < numAccepted; i++)
		tasks.push_back(backend.impl->createTask(msg.getRequester(), msg.getRequestId(), msg.getFirstTask() + i, msg.getMinRequirements()));
	reschedule();
	notifySchedule();
	return numAccepted;
}
