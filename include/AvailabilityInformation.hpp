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

#ifndef RESOURCEINFORMATION_H_
#define RESOURCEINFORMATION_H_

#include "BasicMsg.hpp"
#include "Time.hpp"


/**
 * /brief Availability information.
 *
 * This class contains the availability information provided by the schedulers, and transmits
 * it to the dispatchers. This information is used to locate how many tasks can be sent
 * to a certain set of nodes.
 */
class AvailabilityInformation : public BasicMsg {
public:
    AvailabilityInformation() : sequenceNumber(0), fromSch(true) {}
    virtual ~AvailabilityInformation() {}

    // This is described in BasicMsg
    virtual AvailabilityInformation * clone() const = 0;

    /**
     * Obtains the sequence number of this message. It must be greater than the last message
     * received. Otherwise, this message is ignored.
     * @return The sequence number.
     */
    uint32_t getSeq() const {
        return sequenceNumber;
    }

    /**
     * Sets the sequence number of this message.
     * @param s Sequence number.
     */
    void setSeq(uint32_t s) {
        sequenceNumber = s;
    }

    /**
     * Returns whether the message comes from the scheduler
     */
    bool isFromSch() const {
        return fromSch;
    }

    /**
     * Sets whether the message comes from the scheduler
     */
    void setFromSch(bool f) {
        fromSch = f;
    }

    /// Reduces the size of this availability summary so that it is bounded by a certain limit.
    virtual void reduce() = 0;

    MSGPACK_DEFINE(sequenceNumber, fromSch);
protected:
    uint32_t sequenceNumber;   ///< Sequence number, to provide message ordering.
    bool fromSch;   ///< Whether the message comes from the scheduler or the dispatcher
};

#endif /*RESOURCEINFORMATION_H_*/
