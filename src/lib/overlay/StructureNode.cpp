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
#include "StructureNode.hpp"
#include "Time.hpp"
#include "CommLayer.hpp"
#include "InitStructNodeMsg.hpp"
#include "InsertMsg.hpp"
#include "NewChildMsg.hpp"
#include "NewStrNodeMsg.hpp"
#include "StrNodeNeededMsg.hpp"
#include "UpdateZoneMsg.hpp"
#include "NewFatherMsg.hpp"
#include "AckMsg.hpp"
#include "CommitMsg.hpp"
#include "NackMsg.hpp"
#include "RollbackMsg.hpp"
#include "LeaveMsg.hpp"
using namespace std;


StructureNodeObserver::~StructureNodeObserver() {
    for (vector<StructureNodeObserver *>::iterator it = structureNode.observers.begin();
            it != structureNode.observers.end(); it++)
        if (*it == this) {
            structureNode.observers.erase(it);
            break;
        }
}


void TransactionalZoneDescription::commit() {
    if (changing) {
        actualLink = newLink;
        newLink = CommAddress();
        actualZone = newZone;
        newZone.reset();
        changing = false;
    }
}


void TransactionalZoneDescription::rollback() {
    if (changing) {
        newLink = actualLink;
        newZone = actualZone;
        changing = false;
    }
}


void TransactionalZoneDescription::setZoneFrom(const CommAddress & src, const boost::shared_ptr<ZoneDescription> & i) {
    // If the zone is not changing, the update goes only to the actual values
    if (!changing && actualLink == src) {
        actualZone = i;
    } else if (changing) {
        // If the zone is changing, test first the new values
        // If both links are the same, the new values take precedence so that the changes can be rollbacked
        if (newLink == src) {
            newZone = i;
        } else if (actualLink == src) {
            actualZone = i;
        }
    }
}


std::ostream& operator<<(std::ostream& os, const TransactionalZoneDescription & s) {
    if (!s.changing) {
        os << "c=" << s.actualLink;
        os << " seq=" << s.seq << " ";
        if (s.actualZone.get()) os << *s.actualZone;
        else os << "?";
    } else {
        os << "c=" << s.actualLink;
        os << "/" << s.newLink;
        os << " seq=" << s.seq << " ";
        if (s.actualZone.get()) os << *s.actualZone;
        else os << "?";
        os << "/";
        if (s.newZone.get()) os << *s.newZone;
        else os << "?";
    }
    return os;
}


std::ostream & operator<<(std::ostream& os, const StructureNode & s) {
    os << s.getStatus();
    os << " f=" << s.father;
    os << " seq=" << s.seq << " ";
    if (s.zoneDesc.get()) os << *s.zoneDesc;
    else os << "?";
    os << " " << s.delayedMessages.size() << " waiting";
    return os;
}

string StructureNode::getStatus() const {
    static const char * possibleStates[] = {
        "OFFLINE",
        "START_IN",
        "INIT",
        "ONLINE",
        "ADD_CHILD",
        "CHANGE_FATHER",
        "WAIT_NEWSTR",
        "SPLITTING",
        "WAIT_OFFERS",
        "MERGING",
        "LEAVING_WSN",
        "LEAVING"
    };
    return string(possibleStates[state]);
}


static bool compareZones(const boost::shared_ptr<TransactionalZoneDescription> & l, const boost::shared_ptr<TransactionalZoneDescription> & r) {
    // Orders the nodes by address, with those without resource information first
    return !l->getZone().get() || (r->getZone().get() && l->getZone()->getMinAddress() < r->getZone()->getMinAddress());
}


StructureNode::StructureNode(unsigned int fanout) :
        state(OFFLINE), m(fanout < 2 ? 2 : fanout), level(0),
        seq(1), strNeededTimer(0), transaction(NULL_TRANSACTION_ID) {
}


void StructureNode::notifyFather(TransactionId tid) {
    if (father != CommAddress()) {
        // Pre: zoneDesc.get()
        if (!notifiedZoneDesc.get() || !(*notifiedZoneDesc == *zoneDesc)) {
            notifiedZoneDesc.reset(new ZoneDescription(*zoneDesc));
            Logger::msg("St.RN", DEBUG, "There were changes. Sending update to the father");
            UpdateZoneMsg * u = new UpdateZoneMsg;
            u->setZone(*notifiedZoneDesc);
            u->setSequence(seq++);
            // DEBUG: Follow transactions
            u->setTransactionId(tid);
            CommLayer::getInstance().sendMessage(father, u);
        }
    }
}


void StructureNode::checkFanout() {
    // If we are still in no transaction and all the children have notified, check for size restrictions
    // Nodes do not divide until all childrens have notified, even if the info is not used in the split algo.
    if (transaction == NULL_TRANSACTION_ID && subZones.front()->getZone().get()) {
        if (subZones.size() >= 2*m) {
            Logger::msg("St.RN", DEBUG, "Need to split");
            // Set a transaction id so that the comming messages for this transaction can be identified
            transaction = createRandomId();
            txDriver = CommLayer::getInstance().getLocalAddress();
            // This node needs to split. First look for a new father
            // by sending a StrNodeNeeded
            boost::shared_ptr<StrNodeNeededMsg> snnm(new StrNodeNeededMsg);
            snnm->setWhoNeeds(CommLayer::getInstance().getLocalAddress());
            snnm->setTransactionId(transaction);
            CommLayer::getInstance().sendMessage(CommLayer::getInstance().getLocalAddress(), snnm->clone());
            strNeededTimer = CommLayer::getInstance().setTimer(Duration(60.0), snnm);
            state = WAIT_STR;
        }
        // TODO: What if we have no children!!
        else if (father != CommAddress() && subZones.size() < m) {
            Logger::msg("St.RN", DEBUG, "Need to merge");
            // Undone
        } else if (father == CommAddress() && subZones.size() == 1 && level > 0) {
            // Just leave
            CommLayer::getInstance().sendMessage(subZones.front()->getLink(), new NewFatherMsg);
            state = LEAVING;
        }
    }
}


void StructureNode::recomputeZone() {
    // There must be at least one child
    if (subZones.empty()) return;

    // Make a list of the non-null zones
    list<boost::shared_ptr<ZoneDescription> > tmp;
    for (zoneMutableIterator it = subZones.begin(); it != subZones.end(); it++)
        if ((*it)->getZone().get())
            tmp.push_back((*it)->getZone());
    if (tmp.empty())
        zoneDesc.reset();
    else {
        list<boost::shared_ptr<ZoneDescription> >::iterator i = tmp.begin();
        zoneDesc.reset(new ZoneDescription(**i));
        while (i != tmp.end())
            zoneDesc->aggregate(**(i++));
    }
}


/**
 * An Insertion message, with the address of a node that wants to enter the network.
 *
 * It is received when a ResourceNode wants to enter the network, either sent by that
 * node or by a relaying StructureNode.
 * @param src The source address.
 * @param msg The received message.
 * @param self True when this message is being reprocessed.
 */
template<> void StructureNode::handle(const CommAddress & src, const InsertMsg & msg, bool self) {
    if (msg.isForRN()) return;
    Logger::msg("St.RN", INFO, "Handling InsertMsg from ", src, " for node ", msg.getWho());

    // Check that we are not in the middle of another transaction
    if (transaction != NULL_TRANSACTION_ID) {
        Logger::msg("St.RN", DEBUG, "In the middle of a transaction, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
        return;
    } else if (state == ONLINE && !zoneDesc.get()) {
        Logger::msg("St.RN", DEBUG, "Not enough resource information, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
        return;
    } else if (subZones.size() >= 2*m) {
        Logger::msg("St.RN", DEBUG, "Too many children, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
        return;
    }

    // Check that the state is In Network for this node
    if (state == ONLINE) {
        Logger::msg("St.RN", DEBUG, "We are in network!!");
        if (father != CommAddress())
            Logger::msg("St.RN", DEBUG, "We do have father, which ", (src == father ? "is" : "isn't"), " the sender, ",
                      (self ? "is" : "isn't"), " a self-message and ", (zoneDesc->contains(msg.getWho()) ? "is" : "isn't"),
                      " contained in the zone (", *zoneDesc, ")");
        else
            Logger::msg("St.RN", DEBUG, "We don't have father, ",
                      (self ? "is" : "isn't"), " a self-message and ", (zoneDesc->contains(msg.getWho()) ? "is" : "isn't"),
                      " contained in the zone (", *zoneDesc, ")");
        if (father != CommAddress() && (src != father || self) && !zoneDesc->contains(msg.getWho())) {
            Logger::msg("St.RN", DEBUG, "Send it to the father");
            // If this node is not the root, the message does not come from its father or is a self message, and
            // the address of the new node is not contained in its zone interval, the message is resent to the father node.
            CommLayer::getInstance().sendMessage(father, msg.clone());
            // In any other case, the message goes downwards
        } else if (level) {
            // Figure out which direction should the message take now
            // Take the subzone with minimum distance to the new node value
            zoneMutableIterator it = subZones.begin();
            // Skip zones with null information
            while (!(*it)->getZone().get()) it++;
            zoneMutableIterator direction = it++;
            double minDistance = (*direction)->getZone()->distance(msg.getWho());
            for (; it != subZones.end(); it++) {
                double distance = (*it)->getZone()->distance(msg.getWho());
                if (distance <= minDistance) {
                    minDistance = distance;
                    direction = it;
                }
            }
            if (subZones.front()->getZone().get() || (*direction)->getZone()->contains(msg.getWho())) {
                // If every branch is up to date, or the target branch won't grow
                // just send the insertion
                Logger::msg("St.RN", DEBUG, "Send it downwards to ", (*direction)->getLink());
                // Relay the message to the selected subzone
                CommLayer::getInstance().sendMessage((*direction)->getLink(), msg.clone());
            } else {
                // If there are branches that lack resource information, then delay the message
                Logger::msg("St.RN", DEBUG, "Not enough subZone resource information, delaying.");
                delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
            }
        } else {
            Logger::msg("St.RN", DEBUG, "We insert it");
            // The message reaches the leaves, insert the node in the list
            transaction = msg.getTransactionId();
            txDriver = msg.getWho();
            boost::shared_ptr<TransactionalZoneDescription> newZone(new TransactionalZoneDescription);
            newZone->setLink(msg.getWho());
            fireStartChanges();
            Logger::msg("St.RN", DEBUG, "Add the new father to the list of subZones");
            subZones.push_front(newZone);
            subZones.sort(compareZones);
            // Notify the new node
            AckMsg * am = new AckMsg(transaction);
            am->setForRN(true);
            // Send it to the ResourceNode
            CommLayer::getInstance().sendMessage(msg.getWho(), am);
            state = ADD_CHILD;
        }
    }

    // If this node is not in network and the ResourceNode in the same peer asks to insert, create network
    else if (msg.getWho() == CommLayer::getInstance().getLocalAddress()) {
        Logger::msg("St.RN", DEBUG, "We create network");
        transaction = msg.getTransactionId();
        txDriver = msg.getWho();
        boost::shared_ptr<TransactionalZoneDescription> newZone(new TransactionalZoneDescription);
        newZone->setLink(msg.getWho());
        fireStartChanges();
        Logger::msg("St.RN", DEBUG, "Add the new father to the list of subZones");
        subZones.push_front(newZone);
        // This node is no longer available
        fireAvailabilityChanged(false);
        // It is notified
        AckMsg * am = new AckMsg(transaction);
        am->setForRN(true);
        // Send it to the ResourceNode
        CommLayer::getInstance().sendMessage(msg.getWho(), am);
        state = ADD_CHILD;
    }
}


/**
 * An Update message, which contains the aggregated information of a child zone,
 * with the covered resource information and the number of available Structure nodes.
 *
 * It is received when a child node wants to notify its father about a change in
 * its zone description data.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void StructureNode::handle(const CommAddress & src, const UpdateZoneMsg & msg, bool self) {
    Logger::msg("St.RN", INFO, "Handling UpdateZoneMsg from ", src);

    // Which child does it come from?
    int i = 0; // DEBUG
    for (zoneMutableIterator it = subZones.begin(); it != subZones.end(); it++, i++) {
        if ((*it)->comesFrom(src)) {
            Logger::msg("St.RN", DEBUG, "Comes from child ", i);
            if (!(*it)->testAndSet(msg.getSequence())) {
                Logger::msg("St.RN", DEBUG, "It's old information, skipping");
                return;
            }
            // It comes from child i, update its data
            (*it)->setZoneFrom(src, boost::shared_ptr<ZoneDescription>(new ZoneDescription(msg.getZone())));
            subZones.sort(compareZones);
            // Check if the resulting zone changes
            recomputeZone();
            // If we are in no transaction and there is zone information...
            if (transaction == NULL_TRANSACTION_ID && zoneDesc.get()) {
                // If all the children have notified, check to update
                if (subZones.front()->getZone().get()) {
                    // Check if a new update message must be sent
                    notifyFather(msg.getTransactionId());
                }
                // Resend the delayed messages
                handleDelayedMsgs();

                // Check for size restrictions
                checkFanout();
            }
            break;
        }
    }
}


/**
 * A StructureNode Needed message, that specifies that another Structure node is going
 * to split and needs a new Structure node for half of its subzones.
 *
 * It is received when a Structure node splits. If this node is not already in the network,
 * it will answer to the calling node.
 * @param src The source address.
 * @param msg The received message.
 * @param self True when this message is being reprocessed.
 */
template<> void StructureNode::handle(const CommAddress & src, const StrNodeNeededMsg & msg, bool self) {
    Logger::msg("St.RN", INFO, "Handling StrNodeNeededMsg from ", src, " for node ", msg.getWhoNeeds(), " with transaction ID ", msg.getTransactionId());
    // If this node is already in the network, relay the message
    if (state != OFFLINE && state != START_IN) {
        // Look for the subzone with more available Structure nodes.
        // In case of tie, the "leftmost" father is taken
        zoneMutableIterator it = subZones.begin();
        // Skip zones with null information
        while (it != subZones.end() && !(*it)->getZone().get()) it++;
        // There must be at least one zone
        if (it == subZones.end()) {
            // Wait for an update
            Logger::msg("St.RN", DEBUG, "Not enough information, waiting");
            delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
        } else {
            zoneMutableIterator direction = it++;
            int maxAvailable = (*direction)->getZone()->getAvailableStrNodes();
            int debugDir = 0, i = 1;
            // TODO: Look for the oldest free node
            for (; it != subZones.end(); it++, i++) {
                int available = (*it)->getZone()->getAvailableStrNodes();
                if (available > maxAvailable) {
                    maxAvailable = available;
                    direction = it;
                    debugDir = i;
                }
            }
            if (maxAvailable > 0) {
                Logger::msg("St.RN", DEBUG, "The new structure node is in the child ", debugDir, " with ",
                          (*direction)->getZone()->getAvailableStrNodes(), " available nodes");
                (*direction)->getZone()->setAvailableStrNodes((*direction)->getZone()->getAvailableStrNodes() - 1);
                Logger::msg("St.RN", DEBUG, "Now that child has ", (*direction)->getZone()->getAvailableStrNodes(), " available nodes");
                // Relay the message to the selected subzone
                CommLayer::getInstance().sendMessage((*direction)->getLink(), msg.clone());
            } else {
                Logger::msg("St.RN", DEBUG, "Not enough available nodes in this branch");
                // Otherwise, send it upwards
                if (subZones.front()->getZone().get() && father != CommAddress()) {
                    Logger::msg("St.RN", DEBUG, "Information seems up to date, sending up");
                    CommLayer::getInstance().sendMessage(father, msg.clone());
                } else {
                    Logger::msg("St.RN", DEBUG, "Not enough information or no father, waiting");
                    delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
                }
            }
        }
    }

    // This node is the new Structure node (it should come because the ResourceNode joined the network)
    else if (state == OFFLINE) {
        // Set the transaction ID
        Logger::msg("St.RN", DEBUG, "I am the new structure node");
        fireStartChanges();
        transaction = msg.getTransactionId();
        txDriver = msg.getWhoNeeds();
        fireAvailabilityChanged(false);
        // Send a NewStrNodeMsg to the caller node
        NewStrNodeMsg * nsnm = new NewStrNodeMsg;
        nsnm->setWhoOffers(CommLayer::getInstance().getLocalAddress());
        nsnm->setTransactionId(transaction);
        CommLayer::getInstance().sendMessage(msg.getWhoNeeds(), nsnm);
        state = START_IN;
    }

    else Logger::msg("St.RN", WARN, "Offered to enter the network twice!!");
}


/**
 * A New StructureNode offer message, that specifies that a Structure node is available
 * to be the father of part of the children of this node.
 *
 * It is received when a Structure node splits. If this node is not waiting for this message
 * it will not answer to it.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void StructureNode::handle(const CommAddress & src, const NewStrNodeMsg & msg, bool self) {
    Logger::msg("St.RN", INFO, "Handling NewStrNodeMsg from ", src, " with transaction ID ", msg.getTransactionId());
    // Check if the transaction Id is the same as the StrNodeNeededMsg
    if (transaction == msg.getTransactionId() && state == WAIT_STR) {
        CommLayer::getInstance().cancelTimer(strNeededTimer);
        fireStartChanges();
        // If this node is the root and we have no other node offered
        if (father == CommAddress() && newFather == CommAddress()) {
            Logger::msg("St.RN", DEBUG, src, " will be my new father, need one more node");
            newFather = msg.getWhoOffers();
            // Put the node in the NoAck list
            txMembersNoAck.push_back(AddrService(msg.getWhoOffers(), false));
            // We need another node
            boost::shared_ptr<StrNodeNeededMsg> snnm(new StrNodeNeededMsg);
            snnm->setWhoNeeds(CommLayer::getInstance().getLocalAddress());
            snnm->setTransactionId(transaction);
            CommLayer::getInstance().sendMessage(CommLayer::getInstance().getLocalAddress(), snnm->clone());
            strNeededTimer = CommLayer::getInstance().setTimer(Duration(60.0), snnm);
            // Wait till the next node offer
            return;
        }

        // Initialize the new node
        newBrother = msg.getWhoOffers();
        InitStructNodeMsg * isnm_brother = new InitStructNodeMsg;
        // Same transaction ID
        isnm_brother->setTransactionId(transaction);
        isnm_brother->setLevel(level);

        // Check if we must initialize the father also
        if (father == CommAddress() && newFather != CommAddress()) {
            Logger::msg("St.RN", DEBUG, "Sending the initialization message to the father");
            isnm_brother->setFather(newFather);
            // Initialize it
            InitStructNodeMsg * isnm_father = new InitStructNodeMsg;
            // Same transaction ID
            isnm_father->setTransactionId(transaction);
            // Both children, this node and the new brother
            isnm_father->addChild(CommLayer::getInstance().getLocalAddress());
            isnm_father->addChild(msg.getWhoOffers());
            // And one more level
            isnm_father->setLevel(level + 1);
            CommLayer::getInstance().sendMessage(newFather, isnm_father);
        } else {
            Logger::msg("St.RN", DEBUG, "Sending the new child message to the father");
            isnm_brother->setFather(father);
            // Send the father a NewChildMsg
            NewChildMsg * ncm = new NewChildMsg;
            ncm->setTransactionId(transaction);
            ncm->setChild(msg.getWhoOffers());
            ncm->setSequence(seq++);
            ncm->replaces(false);
            txMembersNoAck.push_back(AddrService(father, false));
            CommLayer::getInstance().sendMessage(father, ncm);
        }

        // Just split the list of children
        Logger::msg("St.RN", DEBUG, "About to Split");
        // TODO: take failures into account: what if a child fails? or the father?

        ////// Divide into two groups
        unsigned int numChildren = subZones.size();
        // Look for the two zones at longest distance
        unsigned int maxi = 0;
        double distance[numChildren][numChildren];
        {
            double maxDist = 0.0;
            unsigned int i = 0;
            for (zoneConstIterator it = subZones.begin(); it != subZones.end(); it++, i++) {
                unsigned int j = 0;
                for (zoneConstIterator jt = subZones.begin(); jt != it; jt++, j++) {
                    distance[i][j] = (*it)->getZone()->distance(*(*jt)->getZone());
                    distance[j][i] = distance[i][j];
                    if (distance[i][j] > maxDist) {
                        maxi = i;
                        maxDist = distance[i][j];
                    }
                }
                distance[i][i] = 0.0;
            }
        }
        // Order the zones in increasing distance to one of that zones
        boost::shared_ptr<TransactionalZoneDescription> zone[numChildren];
        {
            unsigned int pos[numChildren];
            for (unsigned int i = 0; i < numChildren; i++) pos[i] = i;
            for (unsigned int i = 1; i < numChildren; i++) {
                unsigned int j = i, tmp = pos[j];
                for (; j > 0 && distance[maxi][tmp] < distance[maxi][pos[j - 1]]; j--) pos[j] = pos[j - 1];
                pos[j] = tmp;
            }
            unsigned int i = 0;
            for (zoneConstIterator it = subZones.begin(); it != subZones.end(); it++, i++)
                zone[pos[i]] = *it;
        }
        Logger::msg("St.RN", DEBUG, "Separated ", numChildren, " branches into 2 groups");

        // Take half of the points nearest to one of them and make one group.
        // Maintain that group, send the other to the new father
        for (unsigned int i = numChildren / 2; i < numChildren; i++) {
            isnm_brother->addChild(zone[i]->getLink());
            // Put a null address on them
            zone[i]->resetLink();
            Logger::msg("St.RN", DEBUG, "Sending the new father message to child with address ", zone[i]->getLink());
            // Send them a NewFatherMsg
            NewFatherMsg * nfm = new NewFatherMsg;
            nfm->setTransactionId(transaction);
            nfm->setFather(msg.getWhoOffers());
            nfm->setForRN(level == 0);
            txMembersNoAck.push_back(AddrService(zone[i]->getLink(), nfm->isForRN()));
            CommLayer::getInstance().sendMessage(zone[i]->getLink(), nfm);
        }

        // Put the node in the NoAck list
        txMembersNoAck.push_back(AddrService(msg.getWhoOffers(), false));
        // Send it half the children
        CommLayer::getInstance().sendMessage(msg.getWhoOffers(), isnm_brother);
        state = SPLITTING;
    } else if (transaction == msg.getTransactionId() && state == LEAVING_WSN) {
        // We are leaving the network, send an InitStrNode with our info
        CommLayer::getInstance().cancelTimer(strNeededTimer);
        // Initialize the new node
        newBrother = msg.getWhoOffers();
        InitStructNodeMsg * isnm_brother = new InitStructNodeMsg;
        // Same transaction ID
        isnm_brother->setTransactionId(transaction);
        isnm_brother->setLevel(level);

        // Check if we must report the father also
        if (father != CommAddress()) {
            Logger::msg("St.RN", DEBUG, "Sending the new child message to the father");
            isnm_brother->setFather(father);
            // Send the father a NewChildMsg
            NewChildMsg * ncm = new NewChildMsg;
            ncm->setTransactionId(transaction);
            ncm->setChild(msg.getWhoOffers());
            ncm->setSequence(seq++);
            ncm->replaces(true);
            txMembersNoAck.push_back(AddrService(father, false));
            CommLayer::getInstance().sendMessage(father, ncm);
        }

        // Send all the children to the new node
        for (zoneConstIterator it = subZones.begin(); it != subZones.end(); it++) {
            isnm_brother->addChild((*it)->getLink());
            // Put a null address on them
            (*it)->resetLink();
            Logger::msg("St.RN", DEBUG, "Sending the new father message to child with address ", (*it)->getLink());
            // Send them a NewFatherMsg
            NewFatherMsg * nfm = new NewFatherMsg;
            nfm->setTransactionId(transaction);
            nfm->setFather(msg.getWhoOffers());
            nfm->setForRN(level == 0);
            txMembersNoAck.push_back(AddrService((*it)->getLink(), nfm->isForRN()));
            CommLayer::getInstance().sendMessage((*it)->getLink(), nfm);
        }

        // Put the node in the NoAck list
        txMembersNoAck.push_back(AddrService(msg.getWhoOffers(), false));
        // Send it half the children
        CommLayer::getInstance().sendMessage(msg.getWhoOffers(), isnm_brother);
        state = LEAVING;
    }
    // A message with a wrong transaction ID is an error or an obsolet one, rollback it
    else {
        Logger::msg("St.RN", INFO, "Wrong Transaction ID (", transaction, " != ", msg.getTransactionId(), "), revoking");
        CommLayer::getInstance().sendMessage(src, new RollbackMsg(msg.getTransactionId()));
    }
}


/**
 * An Initialize Structure Node message. It contains the information needed
 * by a new Structure Node: the address of the father node (isn.father) and
 * the addresses of the children nodes (isn.child). The node must wait for
 * an Update message to get the information of the resources covered by each
 * child zone.
 *
 * It is received when a Structure node is first introduced in the network.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void StructureNode::handle(const CommAddress & src, const InitStructNodeMsg & msg, bool self) {
    // Check that the transaction Id is the same of the StrNodeNeededMsg
    Logger::msg("St.RN", INFO, "Handling InitStructNodeMsg from ", src, " with transaction ID ", msg.getTransactionId());
    if (transaction == msg.getTransactionId() && state == START_IN) {
        // Get the address of the father node
        if (msg.isFatherValid())
            newFather = msg.getFather();
        else newFather = CommAddress();
        level = msg.getLevel();
        for (unsigned int i = 0; i < msg.getNumChildren(); i++) {
            // Get the address of each child
            boost::shared_ptr<TransactionalZoneDescription> newZone(new TransactionalZoneDescription);
            newZone->setLink(msg.getChild(i));
            subZones.push_back(newZone);
        }
        // No need to sort, as the new zones have no zone info yet
        Logger::msg("St.RN", DEBUG, "Ok, initialised: level ", level, ", ", msg.getNumChildren(),
                  " children waiting, ", (msg.isFatherValid() ? "with " : "without"), " father ");
        // Notify the sender
        CommLayer::getInstance().sendMessage(src, new AckMsg(transaction));
        state = INIT;
    }

    // It is an error to receive this message with a different transaction ID, send NACK
    else {
        Logger::msg("St.RN", INFO, "Wrong Transaction ID (", transaction, " != ", msg.getTransactionId(), "), sending NACK");
        CommLayer::getInstance().sendMessage(src, new NackMsg(msg.getTransactionId()));
    }
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
template<> void StructureNode::handle(const CommAddress & src, const NewFatherMsg & msg, bool self) {
    if (msg.isForRN()) return;
    Logger::msg("St.RN", INFO, "Handling NewFatherMsg from ", src);
    if (state == OFFLINE) Logger::msg("St.RN", WARN, "Trying to change father in Offline state.");

    // Check if we are the driver of another transaction that should finish first
    else if (state == START_IN || state == INIT || state == ADD_CHILD) {
        Logger::msg("St.RN", DEBUG, "In another transaction, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
    }

    else if (src == father) {
        // Any other transaction must be rollbacked, if the sender is the actual father
        if (transaction != NULL_TRANSACTION_ID) rollback();
        txDriver = src;
        // Set the new father
        newFather = msg.getFather();
        fireStartChanges();
        // Start a new transaction
        transaction = msg.getTransactionId();
        state = CHANGE_FATHER;
        // Send an ACK
        CommLayer::getInstance().sendMessage(src, new AckMsg(transaction));
    }

    else {
        Logger::msg("St.RN", INFO, "Message does not come from my father, sending NACK");
        CommLayer::getInstance().sendMessage(src, new NackMsg(msg.getTransactionId()));
    }
}


/**
 * A New Child message, to add a new child node to this one.
 *
 * It is received by the father of a node that splits. The sender must be one
 * this node's children. The new child is not immediately inserted, this node will
 * wait for an Update message that reports the resource information managed by
 * this child.
 * @param src The source address.
 * @param msg The received message.
 * @param self True when this message is being reprocessed.
 */
template<> void StructureNode::handle(const CommAddress & src, const NewChildMsg & msg, bool self) {
    Logger::msg("St.RN", INFO, "Handling NewChildMsg from ", src);
    // Check if we are in another transaction
    if (transaction != NULL_TRANSACTION_ID) {
        Logger::msg("St.RN", DEBUG, "In another transaction, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
    } else if (!msg.replaces() && subZones.size() >= 2*m) {
        Logger::msg("St.RN", DEBUG, "Too many children, delaying.");
        delayedMessages.push_back(AddrMsg(src, boost::shared_ptr<BasicMsg>(msg.clone())));
    } else {
        int i = 0; // DEBUG
        // Look for the child dividing
        for (zoneMutableIterator it = subZones.begin(); it != subZones.end(); it++, i++) {
            if (src == (*it)->getLink()) {
                Logger::msg("St.RN", DEBUG, "Refers to child ", i);
                fireStartChanges();
                // Start a new transaction
                transaction = msg.getTransactionId();
                txDriver = src;
                if (msg.replaces()) {
                    Logger::msg("St.RN", DEBUG, "We have to replace it");
                    // Replace that child with the new one
                    (*it)->setLink(msg.getChild());
                    (*it)->setZone(boost::shared_ptr<ZoneDescription>());
                    subZones.sort(compareZones);
                } else {
                    // Mark that child as changed and invalidate zone info, if it has not been updated
                    if (!(*it)->testAndSet(msg.getSequence())) {
                        Logger::msg("St.RN", DEBUG, "This child has already updated its info");
                    } else {
                        (*it)->setLink((*it)->getLink());
                        (*it)->setZone(boost::shared_ptr<ZoneDescription>());
                    }
                    // Insert the new child in the list, without resource information
                    boost::shared_ptr<TransactionalZoneDescription> newZone(new TransactionalZoneDescription);
                    newZone->setLink(msg.getChild());
                    Logger::msg("St.RN", DEBUG, "Add the new father to the list of subZones");
                    subZones.push_front(newZone);
                    subZones.sort(compareZones);
                }
                // Notify the new node
                CommLayer::getInstance().sendMessage(src, new AckMsg(transaction));
                state = ADD_CHILD;
                return;
            }
        }
    }
}


/**
 * An acknowledge message, part of the two-phase commit protocol
 *
 * It is received by a transaction driver when the sender is ready to commit.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void StructureNode::handle(const CommAddress & src, const AckMsg & msg, bool self) {
    if (msg.isForRN()) return;
    Logger::msg("St.RN", INFO, "Handling AckMessage from ", src, " with transaction ID ", msg.getTransactionId());
    if (transaction == msg.getTransactionId() && txDriver == CommLayer::getInstance().getLocalAddress()) {
        // Look for the sender in the NoAck list
        for (list<AddrService>::iterator it = txMembersNoAck.begin(); it != txMembersNoAck.end(); it++) {
            if (src == (*it).first && msg.isFromRN() == (*it).second) {
                // Put it in the Ack list if it is not there
                list<AddrService>::iterator it2 = txMembersAck.begin();
                for (;it2 != txMembersAck.end(); it2++)
                    if (src == (*it2).first && msg.isFromRN() == (*it2).second) break;
                if (it2 == txMembersAck.end()) txMembersAck.push_back(*it);
                txMembersNoAck.erase(it);
                break;
            }
        }

        if (txMembersNoAck.empty()) {
            commit();
        }
    } else {
        Logger::msg("St.RN", INFO, "Wrong Transaction ID (", transaction, " != ", msg.getTransactionId(), "), revoking");
        RollbackMsg * rm = new RollbackMsg(msg.getTransactionId());
        rm->setForRN(msg.isFromRN());
        CommLayer::getInstance().sendMessage(src, rm);
    }
}


/**
 * A negative acknowledge message, part of the two-phase commit protocol
 *
 * It is received by a transaction driver when the sender is unable to commit.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void StructureNode::handle(const CommAddress & src, const NackMsg & msg, bool self) {
    if (msg.isForRN()) return;
    Logger::msg("St.RN", INFO, "Handling NackMessage from ", src, " with transaction ID ", msg.getTransactionId());
    if (transaction == msg.getTransactionId() && txDriver == CommLayer::getInstance().getLocalAddress()) {
        rollback();
        if (state == ONLINE && zoneDesc.get()) {
            handleDelayedMsgs();
            checkFanout();
        }
    } else Logger::msg("St.RN", INFO, "Wrong Transaction ID (", transaction, " != ", msg.getTransactionId(),
                        ") or not driving a transaction, discarding");
}


/**
 * A commit message, part of the two-phase commit protocol
 *
 * It is received by a transaction participant when changes must be commited.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void StructureNode::handle(const CommAddress & src, const CommitMsg & msg, bool self) {
    if (msg.isForRN()) return;
    Logger::msg("St.RN", INFO, "Handling CommitMessage from ", src, " with transaction ID ", msg.getTransactionId());
    // Check the transaction ID
    if (msg.getTransactionId() == transaction) commit();
    // A commit with a wrong transaction ID is an error, discard it
    else Logger::msg("St.RN", INFO, "Wrong Transaction ID (", transaction, " != ", msg.getTransactionId(), "), discarding");
}


/**
 * A rollback message, part of the two-phase commit protocol
 *
 * It is received by a transaction participant when changes must be undone.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void StructureNode::handle(const CommAddress & src, const RollbackMsg & msg, bool self) {
    if (msg.isForRN()) return;
    Logger::msg("St.RN", INFO, "Handling RollbackMsg from ", src, " with transaction ID ", msg.getTransactionId());
    // Check the transaction ID and the sender
    if (msg.getTransactionId() == transaction && txDriver == src) {
        rollback();
        if (state == ONLINE && zoneDesc.get()) {
            handleDelayedMsgs();
            checkFanout();
        }
    }
    // A rollback with a wrong transaction ID is an error, discard it
    else Logger::msg("St.RN", INFO, "Wrong Transaction ID (", transaction, " != ", msg.getTransactionId(), "), discarding");
}


void StructureNode::commit() {
    Logger::msg("St.RN", INFO, "Commiting changes");

    if (txDriver == CommLayer::getInstance().getLocalAddress()) {
        // Send a CommitMsg to everyone in the ack list
        txMembersAck.unique();
        while (!txMembersAck.empty()) {
            AddrService member = txMembersAck.front();
            txMembersAck.pop_front();
            CommitMsg * cm = new CommitMsg(transaction);
            cm->setForRN(member.second);
            CommLayer::getInstance().sendMessage(member.first, cm);
        }
    }

    list<CommAddress> changes;
    // Commit the changes in the subZones
    for (zoneMutableIterator it = subZones.begin(); it != subZones.end();) {
        if ((*it)->isChanging() && (*it)->getLink() != CommAddress()) changes.push_back((*it)->getLink());
        if ((*it)->isChanging() && (*it)->getNewLink() != CommAddress()) changes.push_back((*it)->getNewLink());
        if ((*it)->isDeletion()) {
            // It is a deletion
            // Take it out from the list
            zoneMutableIterator tmp = it++;
            subZones.erase(tmp);
        } else {
            (*it)->commit();
            it++;
        }
    }

    subZones.sort(compareZones);

    // Commit the change to the father node
    if (newFather != CommAddress()) {
        Logger::msg("St.RN", DEBUG, "The father changed also");
        father = newFather;
        seq = 1;
        newFather = CommAddress();
        notifiedZoneDesc.reset();
        // FIXME!!!
        //fireCommitChanges(true, changes);
        fireCommitChanges(true, true, true);
    } else fireCommitChanges(true, true, true);//fireCommitChanges(false, changes);

    // Clear transaction variables
    txMembersAck.clear();
    TransactionId tmp = transaction;
    transaction = NULL_TRANSACTION_ID;

    // Cancel timeouts
    if (state == WAIT_STR) CommLayer::getInstance().cancelTimer(strNeededTimer);

    if (state == LEAVING) {
        state = OFFLINE;
        fireAvailabilityChanged(true);
    } else {
        state = ONLINE;

        // Recompute the zone values
        recomputeZone();

        if (state == ONLINE && zoneDesc.get()) {
            if (subZones.front()->getZone().get()) {
                // Check if a new update message must be sent
                notifyFather(tmp);
            }
            // Resend the delayed messages
            handleDelayedMsgs();

            // Check for size restrictions
            checkFanout();
        }
    }
}


void StructureNode::rollback() {
    Logger::msg("St.RN", INFO, "Revoking changes");

    if (txDriver == CommLayer::getInstance().getLocalAddress()) {
        // Send a rollback to all the Ack'ed members
        // Not Ack'ed members will be rollbacked when they send the ACK with wrong transaction
        txMembersNoAck.clear();
        while (!txMembersAck.empty()) {
            AddrService member = txMembersAck.front();
            txMembersAck.pop_front();
            Logger::msg("St.RN", DEBUG, "Sending Rollback msg to ", member.first, " service ", member.second);
            RollbackMsg * rm = new RollbackMsg(transaction);
            rm->setForRN(member.second);
            CommLayer::getInstance().sendMessage(member.first, rm);
        }
    }

    // Revoking the changes in the subZones
    // There can only be additions or deletions, but not both of them
    for (zoneMutableIterator it = subZones.begin(); it != subZones.end();) {
        if ((*it)->isDeletion()) {
            // It is a deletion
            (*it)->rollback();
            it++;
        } else if ((*it)->isAddition()) {
            // It is an addition
            // Take it out from the list
            zoneMutableIterator tmp = it++;
            subZones.erase(tmp);
        } else it++;
    }

    // Rollback the change to the father node
    if (newFather != CommAddress()) {
        Logger::msg("St.RN", DEBUG, "The father changed also");
        newFather = CommAddress();
    }
    //fireCommitChanges(false, list<CommAddress>());
    fireCommitChanges(false, true, true);

    // Clear transaction variables
    transaction = NULL_TRANSACTION_ID;

    // Cancel timeouts
    if (state == WAIT_STR) CommLayer::getInstance().cancelTimer(strNeededTimer);

    if (state == START_IN || state == INIT) {
        state = OFFLINE;
        fireAvailabilityChanged(true);
    } else {
        state = ONLINE;
    }
}


void StructureNode::handleDelayedMsgs() {
    // What to do with delayed messages
    unsigned int size = delayedMessages.size();
    for (unsigned int i = 0; i < size && transaction == NULL_TRANSACTION_ID; i++) {
        AddrMsg delayedMsg = delayedMessages.front();
        delayedMessages.pop_front();

        CommAddress & src = delayedMsg.first;
        const BasicMsg & msg = *delayedMsg.second;
        // Check the type of the message
        if (typeid(msg) == typeid(InsertMsg))
            handle(src, static_cast<const InsertMsg &>(msg), true);
        else if (typeid(msg) == typeid(StrNodeNeededMsg))
            handle(src, static_cast<const StrNodeNeededMsg &>(msg), true);
        else if (typeid(msg) == typeid(NewFatherMsg))
            handle(src, static_cast<const NewFatherMsg &>(msg), true);
        else if (typeid(msg) == typeid(NewChildMsg))
            handle(src, static_cast<const NewChildMsg &>(msg), true);
    }
}


#define HANDLE_MESSAGE(x) if (typeid(msg) == typeid(x)) { handle(src, static_cast<const x &>(msg)); return true; }
bool StructureNode::receiveMessage(const CommAddress & src, const BasicMsg & msg) {
    HANDLE_MESSAGE(InitStructNodeMsg);
    HANDLE_MESSAGE(UpdateZoneMsg);
    HANDLE_MESSAGE(InsertMsg);
    HANDLE_MESSAGE(StrNodeNeededMsg);
    HANDLE_MESSAGE(NewStrNodeMsg);
    HANDLE_MESSAGE(NewFatherMsg);
    HANDLE_MESSAGE(NewChildMsg);
    HANDLE_MESSAGE(AckMsg);
    HANDLE_MESSAGE(CommitMsg);
    HANDLE_MESSAGE(NackMsg);
    HANDLE_MESSAGE(RollbackMsg);
    return false;
}
