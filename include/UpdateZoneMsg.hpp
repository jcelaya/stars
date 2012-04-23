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

#ifndef UPDATEZONEMSG_H_
#define UPDATEZONEMSG_H_

#include <boost/shared_ptr.hpp>
#include "TransactionMsg.hpp"
#include "ZoneDescription.hpp"


/**
 * \brief Update notification message for the structure.
 *
 * This message is sent by child nodes to notify its father that some values have changed
 * in its subzone; the father will have then to recalculate the values for its zone.
 * This message contains the information aggregated by each zone.
 */
class UpdateZoneMsg : public TransactionMsg {
	/// Set the basic elements for a Serializable class
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(TransactionMsg) & zone & seq;
	}

	boost::shared_ptr<ZoneDescription> zone;   ///< The information of the resources managed by this node
	uint64_t seq;                       ///< Sequence number, to only apply the last changes

public:
	/// Default constructor
	UpdateZoneMsg() : seq(0) {}
	/// Copy constructor
	UpdateZoneMsg(const UpdateZoneMsg & copy) : TransactionMsg(copy),
			zone(copy.zone.get() ? new ZoneDescription(*copy.zone) : NULL), seq(copy.seq) {}

	// This is documented in BasicMsg
	virtual UpdateZoneMsg * clone() const { return new UpdateZoneMsg(*this); }

	// Getters and Setters

	/**
	 * Returns a pointer to the information of the resources managed by the sender node.
	 * @return Pointer to the information.
	 */
	boost::shared_ptr<ZoneDescription> getZone() const { return zone; }

	/**
	 * Sets the information of the resources managed by the sender node.
	 * @param info The information of the resources managed by the sender node.
	 */
	void setZone(const boost::shared_ptr<ZoneDescription> & info) { zone = info; }

	/**
	 * Returns the sequence number, that lets apply only the last changes when update messages
	 * arrive out of order.
	 * @return The sequence number of this message.
	 */
	uint64_t getSequence() const { return seq; }

	/**
	 * Sets the sequence number.
	 * @param s The sequence number of this message.
	 */
	void setSequence(uint64_t s) { seq = s; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("UpdateZoneMsg"); }
};

#endif /*UPDATEZONEMSG_H_*/
