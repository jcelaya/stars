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

#ifndef UPDATEZONEMSG_H_
#define UPDATEZONEMSG_H_

#include <boost/shared_ptr.hpp>
#include "TransactionMsg.hpp"
#include "ZoneDescription.hpp"


/**
 * \brief Update notification message for the structure.
 *
 * This message is sent by child nodes to notify its father that some values have changed
 * in its subzone; the father will have then to recalculate the values for its zone.
 * This message contains the information aggregated by each zone.
 */
class UpdateZoneMsg : public TransactionMsg {
public:
    MESSAGE_SUBCLASS(UpdateZoneMsg);

    /// Default constructor
    UpdateZoneMsg() : seq(0) {}
    /// Copy constructor
    UpdateZoneMsg(const UpdateZoneMsg & copy) : TransactionMsg(copy),
            zone(copy.zone), seq(copy.seq) {}

    // Getters and Setters

    /**
     * Returns a pointer to the information of the resources managed by the sender node.
     * @return Pointer to the information.
     */
    ZoneDescription getZone() const {
        return zone;
    }

    /**
     * Sets the information of the resources managed by the sender node.
     * @param info The information of the resources managed by the sender node.
     */
    void setZone(const ZoneDescription & info) {
        zone = info;
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

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TransactionMsg &)*this, zone, seq);
private:
    ZoneDescription zone;   ///< The information of the resources managed by this node
    uint64_t seq;                       ///< Sequence number, to only apply the last changes
};

#endif /*UPDATEZONEMSG_H_*/
