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

#ifndef FSPDISPATCHER_H_
#define FSPDISPATCHER_H_

#include <list>
#include "Dispatcher.hpp"
#include "FSPAvailabilityInformation.hpp"
#include "Time.hpp"

using stars::FSPAvailabilityInformation;

/**
 * \brief Resource request dispatcher for execution node requests.
 *
 * This Dispatcher receives TaskBagMsg requests to assign tasks to execution nodes. It also
 * controls the aggregation of TimeConstraintInfo objects in this branch of the tree.
 */
class FSPDispatcher : public Dispatcher<FSPAvailabilityInformation> {
public:
    /**
     * Constructs a QueueBalancingDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    FSPDispatcher(OverlayBranch & b) : Dispatcher(b) {}

    static void setBeta(double b) { beta = b; }

    boost::shared_ptr<FSPAvailabilityInformation> getChildWaitingInfo(int c) const {
        return child[c].waitingInfo;
    }

private:
    static double beta;
    std::list<TaskBagMsg *> delayedRequests;

    // This is documented in Dispatcher.
    virtual void recomputeChildrenInfo();

    double getSlownessLimit() const;

    bool mustGoDown(const CommAddress & src, const TaskBagMsg & msg) const {
        return father.addr == CommAddress() || (!msg.isFromEN() && father.addr == src);
    }

    void updateBranchSlowness(const std::array<double, 2> & branchSlowness);

    bool validInformation() const {
        boost::shared_ptr<FSPAvailabilityInformation> info =
                boost::static_pointer_cast<FSPAvailabilityInformation>(getBranchInfo());
        return info.get() && !info->getSummary().empty();
    }

    // This is documented in Dispatcher.
    virtual void informationUpdated();

    // This is documented in Dispatcher.
    virtual void handle(const CommAddress & src, const TaskBagMsg & msg);
};

#endif /* FSPDISPATCHER_H_ */
