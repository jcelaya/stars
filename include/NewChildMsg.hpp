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

#ifndef NEWCHILDMSG_H_
#define NEWCHILDMSG_H_

#include "TransactionMsg.hpp"
#include "CommAddress.hpp"

/**
 * \brief A notification message of new child for a Structure Node.
 *
 * This message notifies a Structure node that it has a new child node. It is received
 * when a new Structure node is inserted in the network, contains the address of the new
 * Structure node and must be sent by the brother that has divided. After the insertion
 * they will have to send an UpdateMsg.
 */
class NewChildMsg : public TransactionMsg {
public:
    MESSAGE_SUBCLASS(NewChildMsg);

    /**
     * Default constructor.
     */
    NewChildMsg() : seq(0), replace(false) {}
    NewChildMsg(const NewChildMsg & copy) : TransactionMsg(copy), child(copy.child), seq(copy.seq), replace(copy.replace) {}

    // Getters and Setters

    /**
     * Returns the address of the node that has been added.
     * @return The node address.
     */
    const CommAddress & getChild() const {
        return child;
    }

    /**
     * Sets the address of the node that has been added.
     * @param addr The node address.
     */
    void setChild(const CommAddress & addr) {
        child = addr;
    }

    /**
     * Returns the sequence number, that lets apply only the last changes when update messages
     * arrive out of order.
     * @return The sequence number of this message.
     */
    uint64_t getSequence() const {
        return seq;
    }

    /**
     * Sets the sequence number.
     * @param s The sequence number of this message.
     */
    void setSequence(uint64_t s) {
        seq = s;
    }

    /**
     * Returns whether the new child must replace the sender in the list of children.
     * This happens when the sender is leaving the network.
     * @return True if the new children replaces the sender.
     */
    bool replaces() const {
        return replace;
    }

    /**
     * Sets whether the new child must replace the sender in the list of children.
     * @param r True if the new children replaces the sender.
     */
    void replaces(bool r) {
        replace = r;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TransactionMsg &)*this, child, seq, replace);
private:
    CommAddress child;   ///< The new child address
    uint64_t seq;                    ///< Sequence number, to only apply the last changes
    bool replace;                    ///< Tells whether the new child replaces the sender or is added
};

#endif /*NEWCHILDMSG_H_*/
