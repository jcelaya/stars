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
public:
    MESSAGE_SUBCLASS(NewStrNodeMsg);

    /**
     * Default constructor.
     */
    NewStrNodeMsg() {}
    NewStrNodeMsg(const NewStrNodeMsg & copy) : TransactionMsg(copy), whoOffers(copy.whoOffers) {}

    // Getters and Setters

    /**
     * Returns the address of the calling node who is offering itself as a new Structure node.
     * @return The calling node address.
     */
    const CommAddress & getWhoOffers() const {
        return whoOffers;
    }

    /**
     * Sets the address of the calling node.
     * @param addr The calling node address.
     */
    void setWhoOffers(const CommAddress & addr) {
        whoOffers = addr;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TransactionMsg &)*this, whoOffers);
private:
    CommAddress whoOffers; ///< The calling node address
};

#endif /*NEWSTRNODEMSG_H_*/
