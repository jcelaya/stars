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

#ifndef STRUCTURENODE_H_
#define STRUCTURENODE_H_

#include <list>
#include <vector>
#include <iostream>
#include <string>
#include <boost/shared_ptr.hpp>
#include "CommAddress.hpp"
#include "CommLayer.hpp"
#include "ZoneDescription.hpp"
#include "TransactionMsg.hpp"


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


class StructureNode;
/**
 * \brief An Observer pattern for StructureNode events.
 */
class StructureNodeObserver {
protected:
    friend class StructureNode;

    StructureNode & structureNode;

    StructureNodeObserver(StructureNode & sn);

    virtual ~StructureNodeObserver();

    /**
     * Reports a change in the availability of a StructureNode.
     * @param available True if the StructureNode is available.
     */
    virtual void availabilityChanged(bool available) = 0;

    virtual void startChanges() = 0;

    virtual void commitChanges(bool fatherChanged, const std::list<CommAddress> & childChanges) = 0;
};


/**
 * \brief A Structure node service.
 *
 * This class defines the service of STaRS which manages the connection and integrity of the network.
 * Its main goals are:
 *
 * <ul>
 *   <li>Maintain the tree-like structure of the network.</li>
 *   <li>Aggregate the information relative to the execution nodes</li>
 *   <li>Route messages through the network, mainly task batches</li>
 * </ul>
 *
 */
class StructureNode : public Service {
    int state;                                      ///< State of the Structure Manager

    // State variables
    unsigned int m;                                 ///< The minimum fanout of this branch.
    unsigned int level;                             ///< The level of the tree this Structure node lies in.
    uint64_t seq;                                   ///< Update sequence number.
    int strNeededTimer;                             ///< Timer ID for the strNodeNeededMsg.
    boost::shared_ptr<ZoneDescription> zoneDesc;           ///< Description of this zone, by aggregating the child zones
    boost::shared_ptr<ZoneDescription> notifiedZoneDesc;   ///< Description of this zone, as it is notified to the father

    CommAddress father;                 ///< The link to the father node.
    CommAddress newFather;              ///< The link to the new father node.

    typedef std::list<boost::shared_ptr<TransactionalZoneDescription> >::iterator zoneMutableIterator;
    /// The list of subZones, ordered by address.
    std::list<boost::shared_ptr<TransactionalZoneDescription> > subZones;

    TransactionId transaction;              ///< The transaction being prepared right now.
    CommAddress txDriver;                   ///< The driver of the transaction.
    /// A pair of an address and a bool to know if it refers to a RN or a SN
    typedef std::pair<CommAddress, bool> AddrService;
    std::list<AddrService> txMembersNoAck;       ///< Members of the transaction that haven't ACKed yet.
    std::list<AddrService> txMembersAck;         ///< Members of the transaction that already ACKed.
    CommAddress newBrother;     ///< The new brother when splitting.

    typedef std::pair<CommAddress, boost::shared_ptr<BasicMsg> > AddrMsg;
    std::list<AddrMsg> delayedMessages;          ///< Delayed messages and source addresses till the transaction ends.

    friend class StructureNodeObserver;
    std::vector<StructureNodeObserver *> observers;   ///< Change observers.

public:
    void fireAvailabilityChanged(bool available) {
        for (std::vector<StructureNodeObserver *>::iterator it = observers.begin();
                it != observers.end(); it++)(*it)->availabilityChanged(available);
    }

    void fireStartChanges() {
        for (std::vector<StructureNodeObserver *>::iterator it = observers.begin();
                it != observers.end(); it++)(*it)->startChanges();
    }

    void fireCommitChanges(bool fatherChanged, const std::list<CommAddress> & childChanges) {
        for (std::vector<StructureNodeObserver *>::iterator it = observers.begin();
                it != observers.end(); it++)(*it)->commitChanges(fatherChanged, childChanges);
    }

private:
    // Helpers

    std::string getStatus() const;

    /**
     * Checks whether there has been enough changes in the last transaction to notify the father
     * node with an UpdateMsg message.
     * @param tid The transaction ID of the last transaction, to follow them.
     */
    void notifyFather(TransactionId tid);

    /**
     * Checks whether the number of children is below m or over 2m, and starts the apropriate
     * protocol to fix the tree structure.
     */
    void checkFanout();

    /**
     * Recomputes the information of this zone from the information notified by each subzone.
     */
    void recomputeZone();

    /**
     * Commits the changes made by the current transaction.
     */
    void commit();

    /**
     * Undoes the changes made by the current transaction.
     */
    void rollback();

    /**
     * Processes the messages delayed by the last transaction.
     *
     * There are messages that must wait for the current transaction to finish, so they are put
     * in a special message queue. When it finishes, they are unqueued and reprocessed in the same
     * order they arrived.
     */
    void handleDelayedMsgs();

    /**
     * Template method to handle an arriving msg
     */
    template<class M> void handle(const CommAddress & src, const M & msg, bool self = false);

public:
    /// Possible states
    enum {
        OFFLINE = 0,
        // Init process states
        START_IN,
        INIT,
        // Online, no transaction in progress
        ONLINE,
        // Adding a new child
        ADD_CHILD,
        // Changing father node
        CHANGE_FATHER,
        // Splitting process states
        WAIT_STR,
        SPLITTING,
        // Merging process states
        WAIT_OFFERS,
        MERGING,
        // Leaving process states
        LEAVING_WSN,
        LEAVING
    };
    typedef std::list<boost::shared_ptr<TransactionalZoneDescription> >::const_iterator zoneConstIterator;

    /**
     * Constructor: it sets the minumum fanout and the maximum bw for updates.
     * @param fanout Minimum fanout for this branch.
     */
    StructureNode(unsigned int fanout);

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    /**
     * Returns a ZoneIterator object for the first child zone.
     * @return A ZoneIterator object of first child.
     */
    zoneConstIterator getFirstSubZone() const {
        return subZones.begin();
    }

    /**
     * Returns a ZoneIterator object for past the last child zone.
     * @return A ZoneIterator object of past the last child.
     */
    zoneConstIterator getLastSubZone() const {
        return subZones.end();
    }

    /**
     * Returns the level of this StructureNode object in the tree.
     * @return The level of the node.
     */
    bool isRNChildren() const {
        return level == 0;
    }

    bool inNetwork() const {
        return state == ONLINE;
    }

    int getLevel() const {
        return level;
    }

    const CommAddress & getFather() const {
        return father;
    }

    // Debug/Simulation methods

    boost::shared_ptr<const ZoneDescription> getZoneDesc() const {
        return zoneDesc;
    }

    boost::shared_ptr<const ZoneDescription> getNotifiedZoneDesc() const {
        return notifiedZoneDesc;
    }

    boost::shared_ptr<TransactionalZoneDescription> getSubZone(int i) const {
        zoneConstIterator it = subZones.begin();
        for (int j = 0; j < i && it != subZones.end(); j++, it++);
        if (it != subZones.end()) return *it;
        else return boost::shared_ptr<TransactionalZoneDescription>();
    }
    unsigned int getNumChildren() const {
        return subZones.size();
    }

    friend std::ostream & operator<<(std::ostream& os, const StructureNode & s);

    template<class Archive> void serializeState(Archive & ar) {
        // Serialization only works if not in a transaction
        ar & state & m & level & zoneDesc & notifiedZoneDesc & father & seq & subZones;
    }
};


/**
 * Adds a StructureNodeObserver to the list of observers. They are notified when a change occurs.
 * @param o The new observer.
 */
inline StructureNodeObserver::StructureNodeObserver(StructureNode & sn) :
        structureNode(sn) {
    sn.observers.push_back(this);
}

#endif /*STRUCTURENODE_H_*/
