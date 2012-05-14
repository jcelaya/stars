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

#ifndef INSERTMSG_H_
#define INSERTMSG_H_

#include "TransactionMsg.hpp"
#include "CommAddress.hpp"

/**
 * \brief An Execution node Insert message.
 *
 * This class of message notifies that an Execution node wants to enter the network.
 */
class InsertMsg : public TransactionMsg {
    /// Set the basic elements for a Serializable class
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(TransactionMsg) & who & forRN;
    }

    CommAddress who;   ///< The resource node address.
    bool forRN;        ///< To say whether this message is for the ResourceNode or the StructureNode

public:
    /**
     * Sets the service as StructureNodeServiceId.
     */
    InsertMsg() : forRN(false) {}
    ~InsertMsg() {}

    // This is described in BasicMsg
    virtual InsertMsg * clone() const {
        return new InsertMsg(*this);
    }

    // Getters and Setters

    /**
     * Returns the address of the new node which is going to be inserted.
     * @return Address of the new node.
     */
    const CommAddress & getWho() const {
        return who;
    }

    /**
     * Sets the address of the new node which is going to be inserted.
     * @param addr Address of the new node.
     */
    void setWho(const CommAddress & addr) {
        who = addr;
    }

    /**
     * Obtain whether this message is for the ResourceNode or the StructureNode.
     * @return True if the message is for the ResourceNode.
     */
    bool isForRN() const {
        return forRN;
    }

    /**
     * Set whether this message is for the ResourceNode.
     * @param rn True if it is for the ResourceNode.
     */
    void setForRN(bool rn) {
        forRN = rn;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {
        os << "who(" << who << ')';
    }

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("InsertMsg");
    }
};

#endif /*INSERTMSG_H_*/
