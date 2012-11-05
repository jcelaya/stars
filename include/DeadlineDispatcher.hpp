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

#ifndef DeadlineDispatcher_H_
#define DeadlineDispatcher_H_

#include <list>
#include "Dispatcher.hpp"
#include "TimeConstraintInfo.hpp"


/**
 * \brief Resource request dispatcher for execution node requests.
 *
 * This Dispatcher receives TaskBagMsg requests to assign tasks to execution nodes. It also
 * controls the aggregation of TimeConstraintInfo objects in this branch of the tree.
 */
class DeadlineDispatcher : public Dispatcher<TimeConstraintInfo> {
public:
    /**
     * Constructs a DeadlineDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    DeadlineDispatcher(StructureNode & sn) : Dispatcher(sn) {}

    // This is documented in Dispatcher.
    virtual void recomputeInfo();

private:
    struct DecissionInfo;
    struct RecentRequest {
        CommAddress requester;
        int64_t requestId;
        Time when;
        RecentRequest(const CommAddress & r, int64_t id, Time t) : requester(r), requestId(id), when(t) {}
    };

    static const Duration REQUEST_CACHE_TIME;
    static const unsigned int REQUEST_CACHE_SIZE;
    std::list<RecentRequest> recentRequests;

    /**
     * A Task bag allocation request. It is received when a client wants to assign a group
     * of task to a set of available execution nodes.
     * @param src Source node address.
     * @param msg TaskBagMsg message with task group information.
     */
    virtual void handle(const CommAddress & src, const TaskBagMsg & msg);
};

#endif /*DeadlineDispatcher_H_*/
