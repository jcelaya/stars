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

#ifndef TASKMONITORMSG_H_
#define TASKMONITORMSG_H_

#include <vector>
#include <utility>
#include "BasicMsg.hpp"


class TaskMonitorMsg: public BasicMsg {
	/// Set the basic elements for a Serializable class
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(BasicMsg) & tasks & states & heartbeat;
	}

	std::vector<std::pair<int64_t, uint32_t> > tasks;
	std::vector<int32_t> states;
	uint32_t heartbeat;     ///< seconds until next monitoring report

public:
	// This is documented in BasicMsg
	virtual TaskMonitorMsg * clone() const { return new TaskMonitorMsg(*this); }

	/**
	 * Returns the number of tasks
	 */
	unsigned int getNumTasks() const { return tasks.size(); }

	/**
	 * Returns the ID of the ith task contained in this request.
	 * @return ID of task.
	 */
	uint32_t getTaskId(unsigned int i) const { return tasks[i].second; }

	/**
	 * Returns the ID of the ith task contained in this request.
	 * @return ID of task.
	 */
	int64_t getRequestId(unsigned int i) const { return tasks[i].first; }

	/**
	 * Returns the ID of the ith task contained in this request.
	 * @return ID of task.
	 */
	int32_t getTaskState(unsigned int i) const { return states[i]; }

	/**
	 * Adds the ID of a task that is requested to be aborted.
	 * @param n ID of task.
	 */
	void addTask(int64_t rid, uint32_t tid, int32_t s) {
		tasks.push_back(std::make_pair(rid, tid));
		states.push_back(s);
	}

	/**
	 * Returns the number of seconds that are expected until next heartbeat.
	 * @return ID of task.
	 */
	uint32_t getHeartbeat() const { return heartbeat; }

	/**
	 * Sets the number of seconds that are expected until next heartbeat.
	 * @param n ID of task.
	 */
	void setHeartbeat(uint32_t n) { heartbeat = n; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("TaskMonitorMsg"); }
};

#endif /* TASKMONITORMSG_H_ */
