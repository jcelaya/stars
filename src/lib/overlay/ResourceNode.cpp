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

#include "Logger.hpp"
#include "ResourceNode.hpp"
#include "UpdateZoneMsg.hpp"
#include "CommLayer.hpp"
#include "LeaveMsg.hpp"
#include "AckMsg.hpp"
#include "CommitMsg.hpp"
#include "NackMsg.hpp"
#include "RollbackMsg.hpp"
#include "InsertMsg.hpp"
#include "NewFatherMsg.hpp"
#include "InsertCommandMsg.hpp"
#include <cmath>
using namespace std;


#define DEBUG_LOG(x) LogMsg("St.RN", DEBUG) << x
#define INFO_LOG(x) LogMsg("St.RN", INFO) << x


ostream & operator<<(ostream& os, const ResourceNode & e) {
    os << e.getStatus();
    os << " f=" << e.father;
    if (e.transaction != NULL_TRANSACTION_ID) {
        os << "/" << e.newFather;
    }
    os << " seq=" << e.seq;
    os << " " << e.delayedMessages.size() << " waiting";
    return os;
}


string ResourceNode::getStatus() const {
    if (father == CommAddress() && newFather == CommAddress() && transaction == NULL_TRANSACTION_ID)
        return "OFFLINE";
    else if (father != CommAddress() && newFather == CommAddress() && transaction == NULL_TRANSACTION_ID)
        return "ONLINE";
    else if (father == CommAddress() && newFather == CommAddress() && transaction != NULL_TRANSACTION_ID)
        return "START_IN";
    else if (father != CommAddress() && newFather == CommAddress() && transaction != NULL_TRANSACTION_ID)
        return "START_OUT";
    else if (father == CommAddress() && newFather != CommAddress())
        return "INIT_FATHER";
    else if (father != CommAddress() && newFather != CommAddress())
        return "CHANGE_FATHER";
    else
        return "UNKNOWN";
}


ResourceNode::ResourceNode() : seq(1), transaction(NULL_TRANSACTION_ID), availableStrNodes(true) {}


void ResourceNode::notifyFather() {
    if (father != CommAddress()) {
        DEBUG_LOG("There were changes. Sending update to the father");
        ZoneDescription zone;
        zone.setAvailableStrNodes(availableStrNodes ? 1 : 0);
        zone.setMaxAddress(CommLayer::getInstance().getLocalAddress());
        zone.setMinAddress(CommLayer::getInstance().getLocalAddress());
        UpdateZoneMsg * u = new UpdateZoneMsg;
        u->setZone(zone);
        u->setSequence(seq++);
        CommLayer::getInstance().sendMessage(father, u);
    }
}


void ResourceNode::availabilityChanged(bool available) {
    availableStrNodes = available;
    if (transaction == NULL_TRANSACTION_ID) notifyFather();
}


void ResourceNode::commit() {
    INFO_LOG("Commiting changes");
    transaction = NULL_TRANSACTION_ID;

    if (father == CommAddress() || father != newFather) {
        DEBUG_LOG("Father has changed, reporting");
        father = newFather;
        newFather = CommAddress();
        seq = 1;
        notifyFather();
        fireFatherChanged(true);
    }

    // Resend the delayed messages
    handleDelayedMsgs();
}


void ResourceNode::rollback() {
    INFO_LOG("Rollback changes");
    transaction = NULL_TRANSACTION_ID;
    newFather = CommAddress();
    fireFatherChanged(false);

    // Resend the delayed messages
    handleDelayedMsgs();
}


/**
 * A New father message, to change the father of a node.
 *
 * It is received by the children of a node that splits. The sender must be
 * this node's father.
 * @param src The source address.
 * @param msg The received message.
 * @param self True when this message is being reprocessed.
 */
template<> void ResourceNode::handle(const CommAddress & src, const NewFatherMsg & msg, bool self) {
    if (!msg.isForRN()) return;
    INFO_LOG("Handling NewFatherMsg from " << src);
    if (transaction != NULL_TRANSACTION_ID) {
        // If we are in the middle of a change, wait
        DEBUG_LOG("In the middle of a transaction, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
        // Check that the sender is our current father
    } else if (father == src) {
        fireFatherChanging();
        newFather = msg.getFather();
        transaction = msg.getTransactionId();
        AckMsg * am = new AckMsg(transaction);
        am->setFromRN(true);
        CommLayer::getInstance().sendMessage(src, am);
    } else INFO_LOG("It does not come from the father, discarding");
}


/**
 * Acknowledge message, which notifies an ResourceNode that the insert message has been accepted.
 * The StructureNode where it has been inserted is supposed to be the sender.
 *
 * It is sent after a successfull InsertMsg cicle.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void ResourceNode::handle(const CommAddress & src, const AckMsg & msg, bool self) {
    if (!msg.isForRN()) return;
    INFO_LOG("Handling AckMessage from " << src << " with transaction " << msg.getTransactionId());
    // Check the transaction id
    if (msg.getTransactionId() == transaction) {
        newFather = src;
        commit();
        DEBUG_LOG("New father set to " << src);
        // Send a commit message to the new father
        CommitMsg * cm = new CommitMsg(msg.getTransactionId());
        CommLayer::getInstance().sendMessage(src, cm);
    }
    // If the transaction id does not match, it is not a valid ACK message, discard it
    else INFO_LOG("Wrong transaction, discarding");
}


/**
 * A negative acknowledge message, part of the two-phase commit protocol
 *
 * It is received by a transaction driver when the sender is unable to commit.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void ResourceNode::handle(const CommAddress & src, const NackMsg & msg, bool self) {
    if (!msg.isForRN()) return;
    INFO_LOG("Handling NackMessage from " << src << " with transaction " << msg.getTransactionId());
    // Check the transaction id
    if (msg.getTransactionId() == transaction) {
        rollback();
        DEBUG_LOG("Giving up insertion... :_(");
    }
    // If the transaction id does not match, it is not a valid ACK message, discard it
    else INFO_LOG("Wrong transaction, discarding");
}


/**
 * A rollback message, part of the two-phase commit protocol
 *
 * It is received by a transaction participant when changes must be undone.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void ResourceNode::handle(const CommAddress & src, const RollbackMsg & msg, bool self) {
    if (!msg.isForRN()) return;
    INFO_LOG("Handling RollbackMsg from " << src << " with transaction " << msg.getTransactionId());
    // Check the transaction id
    if (msg.getTransactionId() == transaction) {
        rollback();
    }
    // A rollback with a wrong transaction ID is an error, discard it
    else INFO_LOG("Wrong Transaction ID (" << transaction << " != " << msg.getTransactionId() << "), discarding");
}


/**
 * A commit message, part of the two-phase commit protocol
 *
 * It is received by a transaction participant when changes must be commited.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void ResourceNode::handle(const CommAddress & src, const CommitMsg & msg, bool self) {
    if (!msg.isForRN()) return;
    INFO_LOG("Handling CommitMessage from " << src << " with transaction " << msg.getTransactionId());
    // Check the transaction id
    if (msg.getTransactionId() == transaction) {
        commit();
    }
    // A commit with a wrong transaction ID is an error, discard it
    else INFO_LOG("Wrong Transaction ID (" << transaction << " != " << msg.getTransactionId() << "), discarding");
}


/**
 * An Insert message that is sent by an external node to request joining the network.
 *
 * @param src The source address.
 * @param msg The received message.
 * @param self True when this message is being reprocessed.
 */
template<> void ResourceNode::handle(const CommAddress & src, const InsertCommandMsg & msg, bool self) {
    // Check we are not in network
    if (father == CommAddress()) {
        fireFatherChanging();
        InsertMsg * im = new InsertMsg;
        im->setWho(CommLayer::getInstance().getLocalAddress());
        // Start a new transaction
        transaction = createRandomId();
        im->setTransactionId(transaction);
        // The first hop is always to a ResourceNode service,
        // unless the destination is the same peer.
        im->setForRN(msg.getWhere() != CommLayer::getInstance().getLocalAddress());
        INFO_LOG("Sending InsertMsg with transaction " << transaction);
        CommLayer::getInstance().sendMessage(msg.getWhere(), im);
        // TODO: Set a timeout
    }
}


/**
 * An Insert message that is sent by an external node to request joining the network.
 *
 * @param src The source address.
 * @param msg The received message.
 * @param self True when this message is being reprocessed.
 */
template<> void ResourceNode::handle(const CommAddress & src, const InsertMsg & msg, bool self) {
    if (!msg.isForRN()) return;
    INFO_LOG("Handling InsertMsg from " << src);
    if (transaction != NULL_TRANSACTION_ID) {
        // If we are in the middle of a change, wait
        DEBUG_LOG("In the middle of a transaction, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
    } else if (father != CommAddress()) {
        DEBUG_LOG("Sending to the father");
        // If we are in the network, relay the message to our father
        InsertMsg * im = msg.clone();
        im->setForRN(false);
        CommLayer::getInstance().sendMessage(father, im);
    } else INFO_LOG("Nothing to do with it");
}


void ResourceNode::handleDelayedMsgs() {
    // What to do with delayed messages
    while (!delayedMessages.empty() && transaction == NULL_TRANSACTION_ID) {
        AddrMsg delayedMsg = delayedMessages.front();
        delayedMessages.pop_front();

        CommAddress & src = delayedMsg.first;
        const BasicMsg & msg = *delayedMsg.second;
        // Check the type of the message
        if (typeid(msg) == typeid(InsertMsg))
            handle(src, static_cast<const InsertMsg &>(msg), true);
        else if (typeid(msg) == typeid(NewFatherMsg))
            handle(src, static_cast<const NewFatherMsg &>(msg), true);
    }
}


#define HANDLE_MESSAGE(x) if (typeid(msg) == typeid(x)) { handle(src, static_cast<const x &>(msg)); return true; }
bool ResourceNode::receiveMessage(const CommAddress & src, const BasicMsg & msg) {
    HANDLE_MESSAGE(NewFatherMsg)
    HANDLE_MESSAGE(AckMsg);
    HANDLE_MESSAGE(NackMsg);
    HANDLE_MESSAGE(CommitMsg);
    HANDLE_MESSAGE(RollbackMsg);
    HANDLE_MESSAGE(InsertMsg);
    HANDLE_MESSAGE(InsertCommandMsg);
    return false;
}
