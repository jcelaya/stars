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

#ifndef NEWSTRNODEMSG_H_
#define NEWSTRNODEMSG_H_

#include "TransactionMsg.hpp"
#include "CommAddress.hpp"

/**
 * \brief A notification message of Structure node that offers to be inserted in the network.
 *
 * This message notifies a Structure node looking for a new one for half of its children that
 * the sender is available for that mission. This message class just contains the address of
 * the available Structure node.
 */
class NewStrNodeMsg : public TransactionMsg {
	/// Set the basic elements for a Serializable descendant
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(TransactionMsg) & whoOffers;
	}

	CommAddress whoOffers; ///< The calling node address

public:
	/**
	 * Default constructor.
	 */
	NewStrNodeMsg() {}
	NewStrNodeMsg(const NewStrNodeMsg & copy) : TransactionMsg(copy), whoOffers(copy.whoOffers) {}

	// This is documented in BasicMsg
	virtual NewStrNodeMsg * clone() const { return new NewStrNodeMsg(*this); }

	// Getters and Setters

	/**
	 * Returns the address of the calling node who is offering itself as a new Structure node.
	 * @return The calling node address.
	 */
	const CommAddress & getWhoOffers() const { return whoOffers; }

	/**
	 * Sets the address of the calling node.
	 * @param addr The calling node address.
	 */
	void setWhoOffers(const CommAddress & addr) { whoOffers = addr; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("NewStrNodeMsg"); }
};

#endif /*NEWSTRNODEMSG_H_*/
