/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
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

#ifndef SUBMISSIONNODE_H_
#define SUBMISSIONNODE_H_

#include <map>
#include <list>
#include "DispatchCommandMsg.hpp"
#include "CommLayer.hpp"
#include "ResourceNode.hpp"
#include "TaskBagAppDatabase.hpp"


/**
 * \brief A node with request submission functionalities.
 *
 * This kind of node provides the functionality to submit and monitor the requests of the
 * user. Controls the execution of the remote tasks and updates the progress information.
 */
class SubmissionNode : public Service, public ResourceNodeObserver {
    bool inChange;   ///< Whether the father of the ResourceNode is changing
    std::list<std::pair<long int, int> > delayedInstances;

    //std::map<long int, int> timeouts;        ///< Association between requests and timeout timers.
    std::map<long int, unsigned int> remainingTasks;   ///< Remaining tasks per app
    std::map<long int, int> retries;          ///< Number of retries of a request
    std::map<CommAddress, int> heartbeats;    ///< Heartbeat timeouts for each execution node
    /// Number of tasks of each application in each execution node
    std::map<CommAddress, std::map<long int, unsigned int> > remoteTasks;

    TaskBagAppDatabase db;   /// Application database

    /**
     * Template method to handle an arriving msg
     */
    template<class M> void handle(const CommAddress & src, const M & msg);

    // This is documented in ResourceNodeObserver
    void fatherChanging() {
        inChange = true;
    }

    // This is documented in ResourceNodeObserver
    void fatherChanged(bool changed);

    void sendRequest(long int appInstance, int prevRetries);

public:
    /**
     * Constructor, with the ResourceNode to observe.
     * @param rn ResourceNode to register as an observer.
     */
    SubmissionNode(ResourceNode & rn) : ResourceNodeObserver(rn), inChange(false) {}

    // This is documented in Service
    bool receiveMessage(const CommAddress & src, const BasicMsg & msg);

    /**
     * Returns whether there is any ongoing application instance
     */
    bool isIdle() const {
        return remainingTasks.empty();
    }
};

#endif /*SUBMISSIONNODE_H_*/
