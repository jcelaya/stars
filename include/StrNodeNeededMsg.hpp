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
