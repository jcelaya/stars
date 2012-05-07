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

#ifndef TASKEVENTMSG_H_
#define TASKEVENTMSG_H_

#include "BasicMsg.hpp"


/**
 * \brief Notification message of an occurred event related to a task.
 *
 * This kind of message is received by a Scheduler whenever a task
 * wants to notify anything; f. e. whenever a task finishes.
 */
class TaskEventMsg : public BasicMsg {
	/// Set the basic elements for a Serializable descendant
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(BasicMsg) & taskId;
	}

	uint32_t taskId;   ///< ID of the notifier task.

public:
	// This is documented in BasicMsg
	virtual TaskEventMsg * clone() const { return new TaskEventMsg(*this); }

	// Getters and Setters

	/**
	 * Returns the ID of the notifying task. The IDs are local to each execution node,
	 * so this kind of message is always checked for its source address being the
	 * local address.
	 * @return Task ID.
	 */
	uint32_t getTaskId() const { return taskId; }

	/**
	 * Sets the task ID.
	 * @param id New task ID.
	 */
	void setTaskId(uint32_t id) { taskId = id; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("TaskEventMsg"); }
};

#endif /*TASKEVENTMSG_H_*/
