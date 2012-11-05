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
    std::list<std::pair<int64_t, int> > delayedInstances;

    //std::map<int64_t, int> timeouts;        ///< Association between requests and timeout timers.
    std::map<int64_t, unsigned int> remainingTasks;   ///< Remaining tasks per app
    std::map<int64_t, int> retries;          ///< Number of retries of a request
    std::map<CommAddress, int> heartbeats;    ///< Heartbeat timeouts for each execution node
    /// Number of tasks of each application in each execution node
    std::map<CommAddress, std::map<int64_t, unsigned int> > remoteTasks;

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

    void sendRequest(int64_t appInstance, int prevRetries);

    void finishedApp(int64_t appId);

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
