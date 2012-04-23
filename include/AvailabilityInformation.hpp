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

#ifndef RESOURCEINFORMATION_H_
#define RESOURCEINFORMATION_H_

#include "BasicMsg.hpp"
#include "Task.hpp"


/**
 * /brief Availability information.
 *
 * This class contains the availability information provided by the schedulers, and transmits
 * it to the dispatchers. This information is used to locate how many tasks can be sent
 * to a certain set of nodes.
 */
class AvailabilityInformation : public BasicMsg {
public:
	/// Default constructor
	AvailabilityInformation() : seq(0), fromSch(true) {}
	/// Default destructor, virtual
	virtual ~AvailabilityInformation() {}

	// This is described in BasicMsg
	virtual AvailabilityInformation * clone() const = 0;

	/**
	 * Obtains the sequence number of this message. It must be greater than the last message
	 * received. Otherwise, this message is ignored.
	 * @return The sequence number.
	 */
	uint32_t getSeq() const { return seq; }

	/**
	 * Sets the sequence number of this message.
	 * @param s Sequence number.
	 */
	void setSeq(uint32_t s) { seq = s; }

	/**
	 * Returns whether the message comes from the scheduler
	 */
	bool isFromSch() const { return fromSch; }

	/**
	 * Sets whether the message comes from the scheduler
	 */
	void setFromSch(bool f) { fromSch = f; }

	/// Reduces the size of this availability summary so that it is bounded by a certain limit.
	virtual void reduce() = 0;

protected:
	uint32_t seq;   ///< Sequence number, to provide message ordering.
	bool fromSch;   ///< Whether the message comes from the scheduler or the dispatcher

private:
	/// Set the basic elements for a Serializable descendant
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(BasicMsg) & seq & fromSch;
		if (IS_SAVING()) reduce();
	}
};

#endif /*RESOURCEINFORMATION_H_*/
