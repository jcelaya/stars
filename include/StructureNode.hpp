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

#include "OverlayBranch.hpp"
#include "CommAddress.hpp"
#include "CommLayer.hpp"
#include "ZoneDescription.hpp"
#include "TransactionMsg.hpp"
#include "TransactionalZoneDescription.hpp"


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
class StructureNode : public OverlayBranch {
    int state;                                      ///< State of the Structure Manager

    // State variables
    unsigned int m;                                 ///< The minimum fanout of this branch.
    unsigned int level;                             ///< The level of the tree this Structure node lies in.
    uint64_t seq;                                   ///< Update sequence number.
    int strNeededTimer;                             ///< Timer ID for the strNodeNeededMsg.
    std::shared_ptr<ZoneDescription> zoneDesc;           ///< Description of this zone, by aggregating the child zones
    std::shared_ptr<ZoneDescription> notifiedZoneDesc;   ///< Description of this zone, as it is notified to the father

    CommAddress father;                 ///< The link to the father node.
    CommAddress newFather;              ///< The link to the new father node.

    typedef std::list<std::shared_ptr<TransactionalZoneDescription> >::iterator zoneMutableIterator;
    /// The list of subZones, ordered by address.
    std::list<std::shared_ptr<TransactionalZoneDescription> > subZones;

    TransactionId transaction;              ///< The transaction being prepared right now.
    CommAddress txDriver;                   ///< The driver of the transaction.
    /// A pair of an address and a bool to know if it refers to a RN or a SN
    typedef std::pair<CommAddress, bool> AddrService;
    std::list<AddrService> txMembersNoAck;       ///< Members of the transaction that haven't ACKed yet.
    std::list<AddrService> txMembersAck;         ///< Members of the transaction that already ACKed.
    CommAddress newBrother;     ///< The new brother when splitting.

    typedef std::pair<CommAddress, std::shared_ptr<BasicMsg> > AddrMsg;
    std::list<AddrMsg> delayedMessages;          ///< Delayed messages and source addresses till the transaction ends.

    friend class StructureNodeObserver;
    std::vector<StructureNodeObserver *> observers;   ///< Change observers.

public:
    void fireAvailabilityChanged(bool available) {
        for (std::vector<StructureNodeObserver *>::iterator it = observers.begin();
                it != observers.end(); it++)(*it)->availabilityChanged(available);
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
    typedef std::list<std::shared_ptr<TransactionalZoneDescription> >::const_iterator zoneConstIterator;

    /**
     * Constructor: it sets the minumum fanout and the maximum bw for updates.
     * @param fanout Minimum fanout for this branch.
     */
    StructureNode(unsigned int fanout);

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    virtual const CommAddress & getLeftAddress() const {
        return subZones.front()->getLink();
    }

    virtual double getLeftDistance(const CommAddress & src) const {
        return subZones.front()->getZone()->distance(src);
    }

    virtual bool isLeftLeaf() const {
        return level == 0;
    }

    virtual const CommAddress & getRightAddress() const {
        return subZones.back()->getLink();
    }

    virtual double getRightDistance(const CommAddress & src) const {
        return subZones.back()->getZone()->distance(src);
    }

    virtual bool isRightLeaf() const {
        return level == 0;
    }

    bool inNetwork() const {
        return state == ONLINE;
    }

    int getLevel() const {
        return level;
    }

    virtual const CommAddress & getFatherAddress() const {
        return father;
    }

    // Debug/Simulation methods

    std::shared_ptr<const ZoneDescription> getZoneDesc() const {
        return zoneDesc;
    }

    std::shared_ptr<TransactionalZoneDescription> getSubZone(int i) const {
        zoneConstIterator it = subZones.begin();
        for (int j = 0; j < i && it != subZones.end(); j++, it++);
        if (it != subZones.end()) return *it;
        else return std::shared_ptr<TransactionalZoneDescription>();
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
