/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
 *
 *  This file is part of STaRS.
 *
 *  STaRS is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  STaRS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with STaRS; if not, see <http://www.gnu.org/licenses/>.
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
public:
    MESSAGE_SUBCLASS(InsertMsg);

    /**
     * Sets the service as StructureNodeServiceId.
     */
    InsertMsg() : forRN(false) {}
    ~InsertMsg() {}

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

    MSGPACK_DEFINE((TransactionMsg &)*this, who, forRN);
private:
    CommAddress who;   ///< The resource node address.
    bool forRN;        ///< To say whether this message is for the ResourceNode or the StructureNode
};

#endif /*INSERTMSG_H_*/
