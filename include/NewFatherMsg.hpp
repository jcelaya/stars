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

#ifndef NEWFATHERMSG_H_
#define NEWFATHERMSG_H_

#include "TransactionMsg.hpp"
#include "CommAddress.hpp"


/**
 * \brief New Father message.
 *
 * This message is sent to a StructureNode or ResourceNode when its father is changing,
 * in a split of join process.
 */
class NewFatherMsg : public TransactionMsg {
public:
    MESSAGE_SUBCLASS(NewFatherMsg);

    /**
     * Default constructor.
     */
    NewFatherMsg() : forRN(false) {}

    // Getters and Setters

    /**
     * Returns the address of the new father node.
     * @return The node address.
     */
    const CommAddress & getFather() const {
        return father;
    }

    /**
     * Sets the address of the new father node.
     * @param addr The node address.
     */
    void setFather(const CommAddress & addr) {
        father = addr;
    }

    bool isForRN() const {
        return forRN;
    }

    void setForRN(bool rn) {
        forRN = rn;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TransactionMsg &)*this, father, forRN);
private:
    CommAddress father; ///< The new child address
    bool forRN;   ///< To say whether this message is for the ResourceNode or the StructureNode
};

#endif /*NEWFATHERMSG_H_*/
