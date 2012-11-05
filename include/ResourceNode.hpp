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

#ifndef RESOURCENODE_H_
#define RESOURCENODE_H_

#include <vector>
#include <list>
#include <utility>
#include <string>
#include <ostream>
#include <boost/shared_ptr.hpp>
#include "CommLayer.hpp"
#include "StructureNode.hpp"
#include "ZoneDescription.hpp"


class ResourceNode;
/**
 * \brief Observer pattern for changes in a ResourceNode
 *
 * This class provides two methods for other objects to observe a ResourceNode when
 * its father changes. Both the start and end of the transaction is signaled.
 */
class ResourceNodeObserver {
protected:
    friend class ResourceNode;
    ResourceNode & resourceNode;   ///< The observed ResourceNode

    /**
     * Signals that a transaction to change the father node has started.
     */
    virtual void fatherChanging() = 0;

    /**
     * Signals that a transaction to change the father node has finished.
     * @param changed True if the father actually changed.
     */
    virtual void fatherChanged(bool changed) = 0;

public:
    /// Default constructor, registers with the resource node.
    ResourceNodeObserver(ResourceNode & rn);

    /// Default destructor, unregisters.
    virtual ~ResourceNodeObserver();
};


/**
 * \brief Resource manager node.
 *
 * This is the Service that directly manages a resource. It is linked from the leaf StructureNodes.
 */
class ResourceNode : public StructureNodeObserver, public Service {
    CommAddress father;                             ///< StructureNode in charge of this ResourceNode.
    uint64_t seq;                                   ///< Update sequence number.
    TransactionId transaction;                      ///< Transaction ID in use
    CommAddress newFather;                          ///< New StructureNode in charge of this ResourceNode.
    bool availableStrNodes;

    /// A pair of an address and a message
    typedef std::pair<CommAddress, boost::shared_ptr<BasicMsg> > AddrMsg;
    std::list<AddrMsg> delayedMessages;   ///< Delayed messages and source addresses till the transaction ends.

    friend class ResourceNodeObserver;
    std::vector<ResourceNodeObserver *> observers;   ///< List of observers for changes.

public:
    void fireFatherChanging() {
        for (std::vector<ResourceNodeObserver *>::iterator it = observers.begin(); it != observers.end(); it++)(*it)->fatherChanging();
    }

    void fireFatherChanged(bool changed) {
        for (std::vector<ResourceNodeObserver *>::iterator it = observers.begin(); it != observers.end(); it++)(*it)->fatherChanged(changed);
    }

private:

    /**
     * Obtains a textual name of the status this node is in.
     * @return Name of the status.
     */
    std::string getStatus() const;

    // StructureNodeObserver methods
    void availabilityChanged(bool available);

    void startChanges() {}

    void commitChanges(bool fatherChanged, const std::list<CommAddress> & childChanges) {}

    /**
     * Checks whether there has been enough changes in the last transaction to notify the father
     * node with an UpdateMsg message.
     */
    void notifyFather();

    /**
     * Commits the changes made by the current transaction.
     */
    void commit();

    /**
     * Undoes the changes made by the current transaction.
     */
    void rollback();

    /**
     * Template method to handle an arriving msg
     */
    template<class M> void handle(const CommAddress & src, const M & msg, bool self = false);

    /**
     * Processes the messages delayed by the last transaction.
     *
     * There are messages that must wait for the current transaction to finish, so they are put
     * in a special message queue. When it finishes, they are unqueued and reprocessed in the same
     * order they arrived.
     */
    void handleDelayedMsgs();

public:
    /**
     * Registers itself with the CommLayer object.
     */
    ResourceNode(StructureNode & sn);
    ~ResourceNode() {}

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    /**
     * Returns the address of the father node.
     * @return Pointer to the address of the father node. It is null if there is no father node.
     */
    const CommAddress & getFather() const {
        return father;
    }

    // Debug/Simulation methods
    friend std::ostream & operator<<(std::ostream& os, const ResourceNode & e);

    template<class Archive> void serializeState(Archive & ar) {
        // Serialization only works if not in a transaction
        ar & father & seq;
    }
};

inline ResourceNodeObserver::ResourceNodeObserver(ResourceNode & rn) :
        resourceNode(rn) {
    rn.observers.push_back(this);
}

#endif /*RESOURCENODE_H_*/
