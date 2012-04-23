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

#ifndef STRNODENEEDEDMSG_H_
#define STRNODENEEDEDMSG_H_

#include "TransactionMsg.hpp"
#include "CommAddress.hpp"

/**
 * \brief A notification message of Structure node needed.
 *
 * This kind of message is used to look for a Structure node among those Execution nodes
 * with an available Structure node. It contains the address of the Structure node which
 * needs it.
 */
class StrNodeNeededMsg : public TransactionMsg {
	/// Set the basic elements for a Serializable class
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(TransactionMsg) & whoNeeds;
	}

	CommAddress whoNeeds; ///< The calling node address

public:
	/// Default constructor.
	StrNodeNeededMsg() {}
	/// Copy constructor.
	StrNodeNeededMsg(const StrNodeNeededMsg & copy) : TransactionMsg(copy), whoNeeds(copy.whoNeeds) {}

	// This is documented in BasicMsg
	virtual StrNodeNeededMsg * clone() const { return new StrNodeNeededMsg(*this); }

	// Getters and Setters

	/**
	 * Returns the address of the calling node.
	 * @return The calling node address.
	 */
	const CommAddress & getWhoNeeds() const { return whoNeeds; }

	/**
	 * Sets the address of the calling node.
	 * @param addr The calling node address.
	 */
	void setWhoNeeds(const CommAddress & addr) { whoNeeds = addr; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("StrNodeNeededMsg"); }
};

#endif /*STRNODENEEDEDMSG_H_*/
