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

#ifndef TASKBAGMSG_H_
#define TASKBAGMSG_H_

#include "BasicMsg.hpp"
#include "CommAddress.hpp"
#include "TaskDescription.hpp"


/**
 * \brief A tasks assignment request.
 *
 * This class represents an assignment request for a bag of tasks. It includes the first
 * and last id of the assigned tasks, the address of the responsible node and the minumum
 * resource requirements for them.
 */
class TaskBagMsg : public BasicMsg {
	/// Set the basic elements for a Serializable class
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(BasicMsg) & requester & requestId & firstTask & lastTask & minRequirements & forEN & fromEN;
	}

	CommAddress requester;             ///< Requester's address
	int64_t requestId;                 ///< Request ID relative to the requester
	uint32_t firstTask;                ///< Id of the first task in the interval to assign.
	uint32_t lastTask;                 ///< Id of the last task in the interval to assign.
	TaskDescription minRequirements;   ///< Minimum requirements for those tasks.
	bool forEN;                        ///< Whether the message is for the EN or the SN
	bool fromEN;                       ///< Whether the message comes from the EN or the SN

public:
	/// Default constructor
	TaskBagMsg() {}
	/// Copy constructor
	TaskBagMsg(const TaskBagMsg & copy) : BasicMsg(copy), requester(copy.requester), requestId(copy.requestId),
			firstTask(copy.firstTask), lastTask(copy.lastTask), minRequirements(copy.minRequirements), forEN(copy.forEN), fromEN(copy.fromEN) {}

	// This is documented in BasicMsg
	virtual TaskBagMsg * clone() const { return new TaskBagMsg(*this); }

	// Getters and Setters

	/**
	 * Obtains the request ID
	 */
	int64_t getRequestId() const { return requestId; }

	/**
	 * Sets the request ID
	 */
	void setRequestId(int64_t v) { requestId = v; }

	/**
	 * Obtains the address of the requester node.
	 */
	const CommAddress & getRequester() const { return requester; }

	/**
	 * Sets the address of the requester node.
	 */
	void setRequester(const CommAddress & a) { requester = a; }

	/**
	 * Returns the ID of the first task contained in this request.
	 * @return ID of task.
	 */
	uint32_t getFirstTask() const { return firstTask; }

	/**
	 * Sets the ID of the first task that is requested to be assigned.
	 * @param n ID of task.
	 */
	void setFirstTask(uint32_t n) { firstTask = n; }

	/**
	 * Returns the ID of the last task contained in this request.
	 * @return ID of task.
	 */
	uint32_t getLastTask() const { return lastTask; }

	/**
	 * Sets the ID of the last task that is requested to be assigned.
	 * @param n ID of task.
	 */
	void setLastTask(uint32_t n) { lastTask = n; }

	/**
	 * Returns the minimum resource requirements for all the tasks requested.
	 * @return A task description with the most restrictive requirements.
	 */
	const TaskDescription & getMinRequirements() const { return minRequirements; }

	/**
	 * Sets the minimum requirements.
	 * @param min Minimum requirements.
	 */
	void setMinRequirements(const TaskDescription & min) { minRequirements = min; }

	/**
	 * Returns whether this message is for the Scheduler.
	 */
	bool isForEN() const { return forEN; }

	/**
	 * Sets whether this message is for the Scheduler.
	 */
	void setForEN(bool en) { forEN = en; }

	/**
	 * Returns whether this message comes from the SubmissionNode
	 */
	bool isFromEN() const { return fromEN; }

	/**
	 * Sets whether this message comes from the SubmissionNode
	 */
	void setFromEN(bool en) { fromEN = en; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {
		os << "Request " << requestId << " from " << requester << " (" << (fromEN ? "RN" : "SN") << "->" << (forEN ? "RN" : "SN") << "), (" << firstTask << ',' << lastTask << ")";
		// os << *minRequirements;
	}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("TaskBagMsg"); }
};

#endif /*TASKBAGMSG_H_*/
