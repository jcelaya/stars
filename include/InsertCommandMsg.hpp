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

#ifndef INSERTCOMMANDMSG_H_
#define INSERTCOMMANDMSG_H_

#include "BasicMsg.hpp"
#include "CommAddress.hpp"


/**
 * A message to command the ResourceNode to insert itself in the tree.
 */
class InsertCommandMsg : public BasicMsg {
	/// Set the basic elements for a Serializable class
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(BasicMsg) & where;
	}

	CommAddress where;   ///< The destination node address.

public:
	/// Default constructor
	InsertCommandMsg() {}

	// This is described in BasicMsg
	virtual InsertCommandMsg * clone() const { return new InsertCommandMsg(*this); }

	/**
	 * Returns the address of the node which is going to receive the InsertMsg.
	 * @return The address of the node.
	 */
	const CommAddress & getWhere() const { return where; }

	/**
	 * Sets the address of the receiver node.
	 * @param p The address of the node.
	 */
	void setWhere(const CommAddress & w) { where = w; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {
		os << "whr(" << where << ')';
	}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("InsertMsg"); }
};

#endif /*INSERTCOMMANDMSG_H_*/
