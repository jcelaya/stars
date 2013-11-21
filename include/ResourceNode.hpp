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

#include "OverlayLeaf.hpp"
#include "CommLayer.hpp"
#include "StructureNode.hpp"
#include "ZoneDescription.hpp"


/**
 * \brief Resource manager node.
 *
 * This is the Service that directly manages a resource. It is linked from the leaf StructureNodes.
 */
class ResourceNode : public OverlayLeaf {
    CommAddress father;                             ///< StructureNode in charge of this ResourceNode.
    uint64_t seq;                                   ///< Update sequence number.
    TransactionId transaction;                      ///< Transaction ID in use
    CommAddress newFather;                          ///< New StructureNode in charge of this ResourceNode.
    bool availableStrNodes;

    /// A pair of an address and a message
    typedef std::pair<CommAddress, std::shared_ptr<BasicMsg> > AddrMsg;
    std::list<AddrMsg> delayedMessages;   ///< Delayed messages and source addresses till the transaction ends.

    /**
     * Obtains a textual name of the status this node is in.
     * @return Name of the status.
     */
    std::string getStatus() const;

    // StructureNodeObserver methods
    void availabilityChanged(bool available);

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
    ResourceNode();
    ~ResourceNode() {}

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    /**
     * Returns the address of the father node.
     * @return Pointer to the address of the father node. It is null if there is no father node.
     */
    virtual const CommAddress & getFatherAddress() const {
        return father;
    }

    // Debug/Simulation methods
    friend std::ostream & operator<<(std::ostream& os, const ResourceNode & e);

    template<class Archive> void serializeState(Archive & ar) {
        // Serialization only works if not in a transaction
        ar & father & seq;
    }
};

#endif /*RESOURCENODE_H_*/
