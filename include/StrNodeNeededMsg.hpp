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
public:
    MESSAGE_SUBCLASS(StrNodeNeededMsg);
    
    /// Default constructor.
    StrNodeNeededMsg() {}
    /// Copy constructor.
    StrNodeNeededMsg(const StrNodeNeededMsg & copy) : TransactionMsg(copy), whoNeeds(copy.whoNeeds) {}

    // Getters and Setters

    /**
     * Returns the address of the calling node.
     * @return The calling node address.
     */
    const CommAddress & getWhoNeeds() const {
        return whoNeeds;
    }

    /**
     * Sets the address of the calling node.
     * @param addr The calling node address.
     */
    void setWhoNeeds(const CommAddress & addr) {
        whoNeeds = addr;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TransactionMsg &)*this, whoNeeds);
private:
    CommAddress whoNeeds; ///< The calling node address
};

#endif /*STRNODENEEDEDMSG_H_*/
