/*
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

#ifndef TRANSACTIONALZONEDESCRIPTION_HPP_
#define TRANSACTIONALZONEDESCRIPTION_HPP_


#include "CommAddress.hpp"
#include "ZoneDescription.hpp"

/**
 * \brief Zone description class with capability to commit or unroll changes.
 *
 * This class represents the information hold by the StructureNode about its children.
 * It consists of a ZoneDescription object and a link to the child node of this subbranch.
 * It also contains the needed information to allow changes to be commited or rollbacked within
 * a 2PC protocol.
 */
class TransactionalZoneDescription {
public:

    /// Default constructor.
    TransactionalZoneDescription() : changing(false), seq(0) {}

    /**
     * Returns true if the source address is the actual link or the new one.
     * @param src Source address.
     * @return True if src is actualLink or newLink.
     */
    bool comesFrom(const CommAddress & src) {
        return actualLink == src || newLink == src;
    }

    /**
     * Commits the changes made to this object.
     */
    void commit();

    /**
     * Undoes the changes made to this object, so that it remains in the same state it was when the change started.
     */
    void rollback();

    /**
     * Returns whether this object is changing or not.
     * @return True if it is changing.
     */
    bool isChanging() const {
        return changing;
    }

    /**
     * Returns whether this object is changing and the zone is being created.
     * @return True if it is.
     */
    bool isAddition() const {
        return changing && actualLink == CommAddress() && newLink != CommAddress();
    }

    /**
     * Returns whether this object is changing and the zone is being deleted.
     * @return True if it is.
     */
    bool isDeletion() const {
        return changing && actualLink != CommAddress() && newLink == CommAddress();
    }

    /**
     * Checks if the sequence number is greater than the stored one, and updates it.
     * @param s The new sequence number.
     * @return True if the new sequence number was higher than the old one.
     */
    bool testAndSet(uint64_t s) {
        if (seq < s) {
            seq = s;
            return true;
        } else return false;
    }

    // Getters and Setters

    /**
     * Returns the const CommAddress that represents the link to the zone described in this object.
     * @return The address of the link.
     */
    const CommAddress & getLink() const {
        return actualLink;
    }

    /**
     * Returns the CommAddress that represents the link to the zone that will be described in this object
     * when it changes; it is different from actualLink if the description is changing.
     * @return The address of the link.
     */
    const CommAddress & getNewLink() const {
        return newLink;
    }

    /**
     * Sets the new link to a zone, so that a change is started. To set the actual link, commit must be called.
     * @param addr The address of the new link.
     */
    void setLink(const CommAddress & addr) {
        newLink = addr;
        changing = true;
    }
    void resetLink() {
        newLink = CommAddress();
        changing = true;
    }

    /**
     * Returns the zone described in this object.
     * @return This zone.
     */
    const boost::shared_ptr<ZoneDescription> & getZone() const {
        return actualZone;
    }

    /**
     * Sets the zone described in this object.
     * @param i The new zone.
     */
    void setZone(const boost::shared_ptr<ZoneDescription> & i) {
        newZone = i;
        if (!changing) actualZone = i;
    }

    /**
     * Fill a zone description from an UpdateMsg object.
     * @param u UpdateMsg object with the zone information.
     */
    void setZoneFrom(const CommAddress & src, const boost::shared_ptr<ZoneDescription> & i);

    template<class Archive> void serializeState(Archive & ar) {
        // Serialization only works if not in a transaction
        ar & actualLink & actualZone & seq;
    }

private:
    bool changing;                            ///< Whether the zone is changing
    CommAddress actualLink;                   ///< The address of the responsible node
    uint64_t seq;                             ///< Update sequence number.
    CommAddress newLink;                      ///< The new address of the responsible node
    boost::shared_ptr<ZoneDescription> actualZone;   ///< The description of the zone covered by this branch
    boost::shared_ptr<ZoneDescription> newZone;      ///< The description of the zone covered by this branch

    friend std::ostream & operator<<(std::ostream& os, const TransactionalZoneDescription & s);
};


#endif /* TRANSACTIONALZONEDESCRIPTION_HPP_ */
