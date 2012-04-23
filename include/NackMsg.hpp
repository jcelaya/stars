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

#ifndef NACKMSG_H_
#define NACKMSG_H_

#include "TransactionMsg.hpp"

/**
 * Not acknowledgement message in a transaction.
 */
class NackMsg : public TransactionMsg {
	/// Set the basic elements for a Serializable descendant
	SRLZ_API SRLZ_METHOD() {
		ar & SERIALIZE_BASE(TransactionMsg) & forRN;
	}

	bool forRN;   ///< To say whether this message is for the ResourceNode or the StructureNode

public:
	NackMsg(TransactionId trans = NULL_TRANSACTION_ID) : TransactionMsg(trans), forRN(false) {}

	// This is documented in BasicMsg
	virtual NackMsg * clone() const { return new NackMsg(*this); }

	bool isForRN() const { return forRN; }

	void setForRN(bool rn) { forRN = rn; }

	// This is documented in BasicMsg
	void output(std::ostream& os) const {}

	// This is documented in BasicMsg
	std::string getName() const { return std::string("NackMsg"); }
};

#endif /*NACKMSG_H_*/
