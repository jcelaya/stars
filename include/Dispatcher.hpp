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

#include <array>

#include "Logger.hpp"
#include "CommLayer.hpp"
#include "ConfigurationManager.hpp"
#include "OverlayBranch.hpp"
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
    virtual std::shared_ptr<AvailabilityInformation> getBranchInfo() const = 0;

    virtual std::shared_ptr<AvailabilityInformation> getChildInfo(int child) const = 0;
};


template <class T>
class Dispatcher : public DispatcherInterface, public OverlayBranchObserver {
public:
    typedef T availInfoType;

    /**
     * Constructs a DeadlineDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    Dispatcher(OverlayBranch & b) : branch(b),
        updateTimer(0), nextUpdate(), inChange(false) {
        // See if sn is already in the network
        branch.registerObserver(this);
        if (branch.inNetwork()) {
            father.addr = branch.getFatherAddress();
            child[0].addr = branch.getChildAddress(0);
            child[1].addr = branch.getChildAddress(1);
        }
    }

    virtual ~Dispatcher() {}

    // This is documented in Service.
    bool receiveMessage(const CommAddress & src, const BasicMsg & msg) {
        if (dynamic_cast<const TaskBagMsg *>(&msg)) {
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
    virtual std::shared_ptr<AvailabilityInformation> getBranchInfo() const {
        return father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;
    }

    /**
     * Provides the availability information coming from a certain child, given its address.
     * @param child The child address.
     * @return Pointer to the information of that child.
     */
    virtual std::shared_ptr<AvailabilityInformation> getChildInfo(int c) const {
        return child[c].availInfo;
    }

    /**
     * Serializes/unserializes the state of a dispatcher.
     */
    template<class Archive> void serializeState(Archive & ar) {
        // Serialization only works if not in a transaction
        father.serializeState(ar);
        child[0].serializeState(ar);
        child[1].serializeState(ar);
    }

    struct Link {
        CommAddress addr;
        std::shared_ptr<T> availInfo;
        std::shared_ptr<T> waitingInfo;
        std::shared_ptr<T> notifiedInfo;
        bool hasNewInformation;
        Link() : hasNewInformation(true) {}
        Link(const CommAddress & a) : addr(a), hasNewInformation(true) {}
        template<class Archive> void serializeState(Archive & ar) {
            // Serialization only works if not in a transaction
            ar & addr & availInfo & waitingInfo & notifiedInfo;
        }
        unsigned int sendUpdate() {
            if (waitingInfo.get() && !(notifiedInfo.get() && *notifiedInfo == *waitingInfo)) {
                if (!notifiedInfo.get()) {
                    Logger::msg("Dsp", DEBUG, "No notified info");
                } else {
                    Logger::msg("Dsp", DEBUG, "Notified info was ", *notifiedInfo);
                    Logger::msg("Dsp.Compare", DEBUG, "Notified info was different from waiting info");
                }
                updateSequenceNumber();
                notifiedInfo = waitingInfo;
                waitingInfo.reset();
                notifiedInfo->setFromSch(false);
                T * sendMsg = notifiedInfo->clone();
                sendMsg->reduce();
                return CommLayer::getInstance().sendMessage(addr, sendMsg);
            }
            else if (waitingInfo.get() && notifiedInfo.get())
                Logger::msg("Dsp.Compare", DEBUG, "Notified info was equal to waiting info");
            return 0;
        }
        void updateSequenceNumber() {
            if (waitingInfo.get())
                waitingInfo->setSeq(notifiedInfo.get() ? notifiedInfo->getSeq() + 1 : 1);
        }
        bool update(const CommAddress & src, const T & msg) {
            if (addr == src) {
                if (availInfo.get() && availInfo->getSeq() >= msg.getSeq()) {
                    Logger::msg("Dsp", INFO, "Discarding old information: ", availInfo->getSeq(), " >= ", msg.getSeq());
                } else {
                    // Update data
                    availInfo.reset(msg.clone());
                    hasNewInformation = true;
                }
                return true;
            }
            return false;
        }
    };

    /**
     * Calculates the availability information of this branch.
     */
    void recomputeInfo() {
        Logger::msg("Dsp", DEBUG, "Recomputing the branch information");
        recomputeFatherInfo();
        recomputeChildrenInfo();
        father.hasNewInformation = child[0].hasNewInformation = child[1].hasNewInformation = false;
    }

protected:
    OverlayBranch & branch;
    /// Info about the rest of the tree
    Link father;
    /// Info about this branch
    Link child[2];

    typedef std::pair<CommAddress, std::shared_ptr<T> > AddrMsg;
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
        Logger::msg("Dsp", INFO, "Handling AvailabilityInformation from ", src, ": ", msg);

        if (inChange) {
            // Delay this message
            Logger::msg("Dsp", DEBUG, "In the middle of a change, delaying");
            delayedUpdates.push_back(AddrMsg(src, std::shared_ptr<T>(msg.clone())));
        } else if ((!msg.isFromSch() && father.update(src, msg)) || child[0].update(src, msg) || child[1].update(src, msg)) {
            // Check if the resulting zone changes
            if (!delayed) {
                recomputeInfo();
                informationUpdated();
                notify();
            }
        } else {
            Logger::msg("Dsp", INFO, "Comes from unknown node, maybe old info?");
        }
    }

    virtual void informationUpdated() {}

    /**
     * An update timer, to signal when an update is allowed to be sent in order to not overcome
     * a certain bandwidth consumption.
     * @param src Source node address.
     * @param msg UpdateTimer message.
     */
    void handle(const CommAddress & src, const UpdateTimer & msg) {
        Logger::msg("Dsp", INFO, "Handling UpdateTimer");
        updateTimer = 0;
        notify();
    }

    /**
     * Checks whether an update must be sent to the father and children nodes. It also programs a timer
     * if the bandwidth limit is going to be overcome.
     */
    void notify() {
        static std::shared_ptr<UpdateTimer> upMsg(new UpdateTimer);
        if (nextUpdate > Time::getCurrentTime() || updateTimer != 0) {
            Logger::msg("Dsp", DEBUG, "Wait a bit...");
            if (updateTimer == 0) {
                // Program the update timer
                updateTimer = CommLayer::getInstance().setTimer(nextUpdate, upMsg);
            }
        } else {
            // No update timer, we can send update messages
            unsigned int sentSize = 0;
            if (!inChange) {
                if (father.addr != CommAddress()) {
                    unsigned int s = father.sendUpdate();
                    if (s > 0)
                        Logger::msg("Dsp", DEBUG, "There were changes for the father, sending update");
                    sentSize += s;
                }
                // Notify the children
                for (int c : {0, 1}) {
                    if (!branch.isLeaf(c)) {
                        unsigned int s = child[c].sendUpdate();
                        if (s > 0)
                            Logger::msg("Dsp", DEBUG, "There were changes for the ", childName(c), " child, sending update");
                        sentSize += s;
                    }
                }
            }
            double ubw = ConfigurationManager::getInstance().getUpdateBandwidth();
            double t = ubw > 0.0 ? (double)sentSize / ConfigurationManager::getInstance().getUpdateBandwidth() : 0.0;
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

    void sendTasks(const TaskBagMsg & msg, const std::array<unsigned int, 2> & numTasks, bool dontSendToFather) {
        // Now create and send the messages
        unsigned int nextTask = msg.getFirstTask();
        std::shared_ptr<T> availInfo = father.waitingInfo.get() ? father.waitingInfo : father.notifiedInfo;

        for (int c : {0, 1}) {
            if (numTasks[c] > 0) {
                Logger::msg("Dsp", INFO, "Sending ", numTasks[c], " tasks to the ", childName(c), " child (", child[c].addr, ')');
                TaskBagMsg * tbm = msg.getSubRequest(nextTask, nextTask + numTasks[c] - 1);
                tbm->setInfoSequenceUsed(child[c].availInfo->getSeq());
                tbm->setForEN(branch.isLeaf(c));
                nextTask += numTasks[c];
                CommLayer::getInstance().sendMessage(child[c].addr, tbm);
            }
        }

        // If this branch cannot execute all the tasks, send the request to the father
        if (nextTask <= msg.getLastTask()) {
            Logger::msg("Dsp", DEBUG, "There are ", msg.getLastTask() - (nextTask - 1), " remaining tasks for the father (", father.addr, ')');
            if (branch.getFatherAddress() != CommAddress()) {
                if (dontSendToFather) {
                    Logger::msg("Dsp", DEBUG, "But came from the father.");
                } else {
                    TaskBagMsg * tbm = msg.getSubRequest(nextTask, msg.getLastTask());
                    tbm->setInfoSequenceUsed(availInfo->getSeq());
                    CommLayer::getInstance().sendMessage(branch.getFatherAddress(), tbm);
                }
            } else {
                Logger::msg("Dsp", WARN, "Discarding ", msg.getLastTask() - (nextTask - 1), " because we are the root");
            }
        }
    }

    bool checkState() const {
        if (!branch.inNetwork()) {
            Logger::msg("Dsp", WARN, "Not in network.");
            return false;
        }
        if (!father.waitingInfo.get() && !father.notifiedInfo.get()) {
            Logger::msg("Dsp", WARN, "No availability information.");
            return false;
        }
        return true;
    }

    void showMsgSource(const CommAddress & src, const TaskBagMsg & msg) const {
        if (!msg.isFromEN() && src == father.addr) {
            Logger::msg("Dsp", INFO, "Received a TaskBagMsg from ", src, " (father)");
        } else {
            Logger::msg("Dsp", INFO, "Received a TaskBagMsg from ", src, " (", childName(src == child[0].addr ? 0 : 1), " child)");
        }
        const TaskDescription & req = msg.getMinRequirements();
        unsigned int numTasksReq = msg.getLastTask() - msg.getFirstTask() + 1;
        uint64_t a = req.getLength();
        Logger::msg("Dsp.FSP", INFO, "Request ", msg.getRequestId(), " from ", msg.getRequester(), " with ", numTasksReq, " tasks with requirements:");
        Logger::msg("Dsp.FSP", INFO, "Memory: ", req.getMaxMemory(), "   Disk: ", req.getMaxDisk(), "   Length: ", a);
    }

    virtual void recomputeFatherInfo() {
        if (child[0].hasNewInformation || child[1].hasNewInformation) {
            if (child[0].availInfo.get()) {
                father.waitingInfo.reset(child[0].availInfo->clone());
                if (child[1].availInfo.get())
                    father.waitingInfo->join(*child[1].availInfo);
                father.updateSequenceNumber();
                Logger::msg("Dsp", DEBUG, "The result is ", *father.waitingInfo);
            } else if (child[1].availInfo.get()) {
                father.waitingInfo.reset(child[1].availInfo->clone());
                father.updateSequenceNumber();
                Logger::msg("Dsp", DEBUG, "The result is ", *father.waitingInfo);
            } else
                father.waitingInfo.reset();
        }
    }

    virtual void recomputeChildrenInfo() {}

    static const char * childName(int i) {
        static const char * name[2] = { "left", "right" };
        return name[i];
    }

private:
    // This is documented in OverlayBranchObserver
    virtual void startChanges() {
        inChange = true;
    }

    // This is documented in OverlayBranchObserver
    virtual void commitChanges(bool fatherChanged, bool leftChanged, bool rightChanged) {
        inChange = false;
        if (fatherChanged)
            father = Link(branch.getFatherAddress());
        if (leftChanged)
            child[0] = Link(branch.getChildAddress(0));
        if (rightChanged)
            child[1] = Link(branch.getChildAddress(1));
        // Check delayed updates
        for (typename std::vector<AddrMsg>::iterator it = delayedUpdates.begin();
                it != delayedUpdates.end(); it++)
            handle(it->first, *it->second, true);
        delayedUpdates.clear();
        recomputeInfo();
        notify();
    }
};

#endif /* DISPATCHER_H_ */
