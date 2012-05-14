/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
 *
 *  This file is part of PeerComp.
 *
 *  PeerComp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  PeerComp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PeerComp; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef MINSTRETCHDISPATCHER_H_
#define MINSTRETCHDISPATCHER_H_

#include "Dispatcher.hpp"
#include "StretchInformation.hpp"
#include "Time.hpp"


/**
 * \brief Resource request dispatcher for execution node requests.
 *
 * This Dispatcher receives TaskBagMsg requests to assign tasks to execution nodes. It also
 * controls the aggregation of TimeConstraintInfo objects in this branch of the tree.
 */
class MinStretchDispatcher : public Dispatcher<StretchInformation> {
public:
    /**
     * Constructs a QueueBalancingDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    MinStretchDispatcher(StructureNode & sn) : Dispatcher(sn) {}

private:
    // This is documented in Dispatcher.
    virtual void handle(const CommAddress & src, const TaskBagMsg & msg);

    // This is documented in Dispatcher.
    virtual void recomputeInfo();
};



#if 0
#include <vector>
#include <utility>
#include <list>
#include <boost/shared_ptr.hpp>
#include "StructureNode.hpp"
#include "CommLayer.hpp"
#include "StretchInformation.hpp"
#include "TaskBagMsg.hpp"
#include "UpdateTimer.hpp"


/**
 * \brief A dispatcher that tries to provide fairness.
 *
 * This dispatcher uses the information about the stretch comming from the execution nodes
 * to load balance the system and maintain a constant limit in the relation between the
 * maximum and minimum stretch.
 */
class MinStretchDispatcher: public Service, public StructureNodeObserver {
public:
    typedef std::pair<CommAddress, boost::shared_ptr<StretchInformation> > Child;

private:
    // Incoming info
    std::vector<Child> children;                      ///< Information about children nodes and their address
    boost::shared_ptr<StretchInformation> fatherInfo;   ///< Information of the rest of the tree

    typedef std::pair<CommAddress, boost::shared_ptr<BasicMsg> > AddrMsg;
    std::vector<AddrMsg> delayedUpdates;   ///< Delayed messages when the structure is changing

    // Outgoing info
    boost::shared_ptr<StretchInformation> ntfFatherInfo;           ///< Information sent to the father
    boost::shared_ptr<StretchInformation> waitingInfo;             ///< Information to be sent
    std::vector<boost::shared_ptr<StretchInformation> > ntfChildInfo;   ///< Information sent to the children

    int updateTimer;   ///< Timer for the next UpdateMsg to be sent

    bool inChange;     ///< States whether the StructureNode links are changing

    /**
     * A Task bag allocation request. It is received when a client wants to assign a group
     * of task to a set of available execution nodes.
     * @param src Source node address.
     * @param msg TaskBagMsg message with task group information.
     */
    void handle(const CommAddress & src, const TaskBagMsg & msg);

    /**
     * An update of the availability of a subzone of the tree.
     * @param src Source node address.
     * @param msg StretchInformation with the updated availability information.
     * @param self True if the message has been delayed.
     */
    void handle(const CommAddress & src, const StretchInformation & msg, bool self = false);

    /**
     * An update timer, to signal when an update is allowed to be sent in order to not overcome
     * a certain bandwidth consumption.
     * @param src Source node address.
     * @param msg UpdateTimer message.
     */
    void handle(const CommAddress & src, const UpdateTimer & msg);

    /**
     * Checks whether an update must be sent to the father node. It also programs a timer
     * if the bandwidth limit is going to be overcome.
     */
    void notifyFather();

    /**
     * Checks whether an update must be sent to the children nodes.
     */
    void notifyChildren();

    /**
     * Calculates the availability information of this branch.
     */
    void recomputeInfo();

    // This is documented in StructureNodeObserver
    void availabilityChanged(bool available) {}

    // This is documented in StructureNodeObserver
    void startChanges();

    // This is documented in StructureNodeObserver
    void commitChanges(bool fatherChanged, const std::list<CommAddress> & childChanges);

public:
    /**
     * Constructs a DeadlineDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    MinStretchDispatcher(StructureNode & sn) :
            StructureNodeObserver(sn), updateTimer(0), inChange(false) {}

    // This is documented in Service
    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    /**
     * Provides the information for this branch.
     * @returns Pointer to the calculated StretchInformation.
     */
    const boost::shared_ptr<StretchInformation> & getBranchInfo() const {
        return waitingInfo;
    }

    /**
     * Provides the information for this branch.
     * @returns Pointer to the sent StretchInformation.
     */
    const boost::shared_ptr<StretchInformation> & getNotifiedInfo() const {
        return ntfFatherInfo;
    }

    /**
     * Provides the availability information coming from a certain child, given its address.
     * @param child The child address.
     * @return Pointer to the information of that child.
     */
    boost::shared_ptr<StretchInformation> getChildInfo(const CommAddress & child) const {
        for (std::vector<Child>::const_iterator it = children.begin(); it != children.end(); it++)
            if (it->first == child) return it->second;
        return boost::shared_ptr<StretchInformation>();
    }

    template<class Archive> void serializeState(Archive & ar) {
        // Only works in stable: no change, no delayed update
        ar & children & fatherInfo & ntfFatherInfo & ntfChildInfo & waitingInfo;
    }
};
#endif

#endif /* MINSTRETCHDISPATCHER_H_ */
