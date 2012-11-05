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

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include <boost/shared_ptr.hpp>
#include "Logger.hpp"
#include "CommLayer.hpp"
#include "ConfigurationManager.hpp"
#include "StructureNode.hpp"
#include "TaskBagMsg.hpp"
#include "UpdateTimer.hpp"
class AvailabilityInformation;


class DispatcherInterface : public Service {
public:
    virtual ~DispatcherInterface() {}

    /**
     * Provides the information for this branch.
     * @returns Pointer to the calculated availability information.
     */
    virtual boost::shared_ptr<AvailabilityInformation> getBranchInfo() const = 0;

    /**
     * Provides the availability information coming from a certain child, given its address.
     * @param child The child address.
     * @return Pointer to the information of that child.
     */
    virtual boost::shared_ptr<AvailabilityInformation> getChildInfo(const CommAddress & child) const = 0;

    virtual bool changed() = 0;
};


template <class T>
class Dispatcher : public DispatcherInterface, public StructureNodeObserver {
public:
    typedef T availInfoType;

    /**
     * Constructs a DeadlineDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    Dispatcher(StructureNode & sn) :
            StructureNodeObserver(sn), infoChanged(false), updateTimer(0), nextUpdate(), inChange(false) {
        // See if sn is already in the network
        if (sn.inNetwork()) {
            father.addr = sn.getFather();
            children.resize(sn.getNumChildren());
            for (size_t i = 0; i < children.size(); ++i)
                children[i].addr = sn.getSubZone(i)->getLink();
        }
    }

    virtual ~Dispatcher() {}

    // This is documented in Service.
    bool receiveMessage(const CommAddress & src, const BasicMsg & msg) {
        if (typeid(msg) == typeid(TaskBagMsg)) {
            handle(src, static_cast<const TaskBagMsg &>(msg));
            return true;
        } else if (typeid(msg) == typeid(UpdateTimer)) {
            handle(src, static_cast<const UpdateTimer &>(msg));
            return true;
        } else if (typeid(msg) == typeid(T)) {
            handle(src, static_cast<const T &>(msg));
            return true;
        } else return false;
    }

    // This is documented in DispatcherInterface.
    virtual boost::shared_ptr<AvailabilityInformation> getBranchInfo() const {
        return father.notifiedInfo.get() ? father.notifiedInfo : father.waitingInfo;
    }

    /**
     * Provides the availability information coming from a certain child, given its address.
     * @param child The child address.
     * @return Pointer to the information of that child.
     */
    virtual boost::shared_ptr<AvailabilityInformation> getChildInfo(const CommAddress & child) const {
        for (typename std::vector<Link>::const_iterator it = children.begin(); it != children.end(); it++)
            if (it->addr == child) return it->availInfo;
        return boost::shared_ptr<AvailabilityInformation>();
    }

    /**
     * Serializes/unserializes the state of a dispatcher.
     */
    template<class Archive> void serializeState(Archive & ar) {
        // Serialization only works if not in a transaction
        father.serializeState(ar);
        size_t s = children.size();
        ar & s;
        children.resize(s);
        for (size_t i = 0; i < s; ++i) {
            children[i].serializeState(ar);
        }
    }

    struct Link {
        CommAddress addr;
        boost::shared_ptr<T> availInfo;
        boost::shared_ptr<T> waitingInfo;
        boost::shared_ptr<T> notifiedInfo;
        Link() {}
        Link(const CommAddress & a) : addr(a) {}
        template<class Archive> void serializeState(Archive & ar) {
            // Serialization only works if not in a transaction
            ar & addr & availInfo & waitingInfo & notifiedInfo;
        }
    };

    virtual bool changed() {
        bool tmp = infoChanged;
        infoChanged = false;
        return tmp;
    }

    /**
     * Calculates the availability information of this branch.
     */
    virtual void recomputeInfo() = 0;

protected:
    /// Info about the rest of the tree
    Link father;
    /// Info about this branch
    std::vector<Link> children;
    bool infoChanged;

    typedef std::pair<CommAddress, boost::shared_ptr<T> > AddrMsg;
    std::vector<AddrMsg> delayedUpdates;   ///< Delayed messages when the structure is changing

    int updateTimer;   ///< Timer for the next UpdateMsg to be sent
    Time nextUpdate;   ///< Time before which next update must not be sent

    bool inChange;     ///< States whether the StructureNode links are changing

    /**
     * An update of the availability of a subzone of the tree.
     * @param src Source node address.
     * @param msg Instance of an AvailabilityInformation subclass with the updated availability information.
     * @param delayed True if the message has been delayed.
     */
    void handle(const CommAddress & src, const T & msg, bool delayed = false) {
        LogMsg("Dsp", INFO) << "Handling AvailabilityInformation from " << src << ": " << msg;

        if (inChange) {
            // Delay this message
            LogMsg("Dsp", DEBUG) << "In the middle of a change, delaying";
            delayedUpdates.push_back(AddrMsg(src, boost::shared_ptr<T>(msg.clone())));
            return;
        }

        // Does it come from the father?
        if (!msg.isFromSch() && father.addr == src) {
            LogMsg("Dsp", DEBUG) << "Comes from the father";
            if (father.availInfo.get() && father.availInfo->getSeq() >= msg.getSeq()) {
                LogMsg("Dsp", INFO) << "Discarding old information: " << father.availInfo->getSeq() << " >= " << msg.getSeq();
                return;
            }
            // Update data
            father.availInfo.reset(msg.clone());
            // Check if the resulting zone changes
            if (!delayed) {
                recomputeInfo();
                notify();
            }
            return;
        }

        else {
            // Which child does it come from?
            for (unsigned int i = 0; i < children.size(); i++) {
                if (children[i].addr == src) {
                    LogMsg("Dsp", DEBUG) << "Comes from child " << i;
                    // Check sequence number, discard old information
                    if (children[i].availInfo.get() && children[i].availInfo->getSeq() >= msg.getSeq()) {
                        LogMsg("Dsp", INFO) << "Discarding old information: " << children[i].availInfo->getSeq() << " >= " << msg.getSeq();
                        return;
                    }
                    // It comes from child i, update its data
                    children[i].availInfo.reset(msg.clone());
                    // Check if the resulting zone changes
                    if (!delayed) {
                        recomputeInfo();
                        notify();
                    }
                    return;
                }
            }
        }

        LogMsg("Dsp", INFO) << "Comes from unknown node, maybe old info?";
    }

    /**
     * An update timer, to signal when an update is allowed to be sent in order to not overcome
     * a certain bandwidth consumption.
     * @param src Source node address.
     * @param msg UpdateTimer message.
     */
    void handle(const CommAddress & src, const UpdateTimer & msg) {
        LogMsg("Dsp", INFO) << "Handling UpdateTimer";
        updateTimer = 0;
        notify();
    }

    /**
     * Checks whether an update must be sent to the father and children nodes. It also programs a timer
     * if the bandwidth limit is going to be overcome.
     */
    void notify() {
        static boost::shared_ptr<UpdateTimer> upMsg(new UpdateTimer);
        if (nextUpdate > Time::getCurrentTime() || updateTimer != 0) {
            LogMsg("Dsp", DEBUG) << "Wait a bit...";
            if (updateTimer == 0) {
                // Program the update timer
                updateTimer = CommLayer::getInstance().setTimer(nextUpdate, upMsg);
            }
        } else {
            // No update timer, we can send update messages
            unsigned int sentSize = 0;
            if (!inChange && father.addr != CommAddress() && father.waitingInfo.get() &&
                    !(father.notifiedInfo.get() && *father.notifiedInfo == *father.waitingInfo)) {
                LogMsg("Dsp", DEBUG) << "There were changes for the father, sending update";
                uint32_t seq = father.notifiedInfo.get() ? father.notifiedInfo->getSeq() + 1 : 1;
                father.notifiedInfo = father.waitingInfo;
                father.waitingInfo.reset();
                father.notifiedInfo->setSeq(seq);
                father.notifiedInfo->setFromSch(false);
                T * sendMsg = father.notifiedInfo->clone();
                sendMsg->reduce();
                sentSize += CommLayer::getInstance().sendMessage(father.addr, sendMsg);
            }
            if (!inChange && !structureNode.isRNChildren()) {
                // Notify the children
                for (unsigned int i = 0; i < children.size(); i++) {
                    if (children[i].waitingInfo.get() &&
                            !(children[i].notifiedInfo.get() && *children[i].notifiedInfo == *children[i].waitingInfo)) {
                        LogMsg("Dsp", DEBUG) << "There were changes with children " << i << ", sending update";
                        uint32_t seq = children[i].notifiedInfo.get() ? children[i].notifiedInfo->getSeq() + 1 : 1;
                        children[i].notifiedInfo = children[i].waitingInfo;
                        children[i].waitingInfo.reset();
                        children[i].notifiedInfo->setSeq(seq);
                        children[i].notifiedInfo->setFromSch(false);
                        T * sendMsg = children[i].notifiedInfo->clone();
                        sendMsg->reduce();
                        sentSize += CommLayer::getInstance().sendMessage(children[i].addr, sendMsg);
                    }
                }
            }
            double t = (double)sentSize / ConfigurationManager::getInstance().getUpdateBandwidth();
            nextUpdate = Time::getCurrentTime() + Duration(t);
        }
    }

    /**
     * A Task bag allocation request. It is received when a client wants to assign a group
     * of task to a set of available execution nodes.
     * @param src Source node address.
     * @param msg TaskBagMsg message with task group information.
     */
    virtual void handle(const CommAddress & src, const TaskBagMsg & msg) = 0;

private:
    // This is documented in StructureNodeObserver
    virtual void availabilityChanged(bool available) {}

    // This is documented in StructureNodeObserver
    virtual void startChanges() {
        inChange = true;
    }

    // This is documented in StructureNodeObserver
    virtual void commitChanges(bool fatherChanged, const std::list<CommAddress> & childChanges) {
        inChange = false;
        if (fatherChanged) {
            // Force an update
            father.addr = structureNode.getFather();
            father.notifiedInfo.reset();
        }
        for (std::list<CommAddress>::const_iterator child = childChanges.begin(); child != childChanges.end(); child++) {
            // Look for it in the list of children
            bool notFound = true;
            for (typename std::vector<Link>::iterator it = children.begin(); it != children.end(); it++) {
                if (it->addr == *child) {
                    LogMsg("Dsp", DEBUG) << "Child " << *child << " is no more";
                    // Remove it
                    children.erase(it);
                    notFound = false;
                    break;
                }
            }
            if (notFound) {
                LogMsg("Dsp", DEBUG) << "New child " << *child;
                // New child, add it at the end
                children.push_back(Link(*child));
            }
        }
        // Check delayed updates
        for (typename std::vector<AddrMsg>::iterator it = delayedUpdates.begin();
                it != delayedUpdates.end(); it++)
            handle(it->first, *it->second, true);
        delayedUpdates.clear();
        // Recompute info
        recomputeInfo();
        // Notify
        notify();
    }
};

#endif /* DISPATCHER_H_ */
