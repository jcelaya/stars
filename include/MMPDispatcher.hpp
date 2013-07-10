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

#ifndef MMPDISPATCHER_H_
#define MMPDISPATCHER_H_

#include "Dispatcher.hpp"
#include "MMPAvailabilityInformation.hpp"
#include "Time.hpp"


/**
 * \brief Resource request dispatcher for execution node requests.
 *
 * This Dispatcher receives TaskBagMsg requests to assign tasks to execution nodes. It also
 * controls the aggregation of TimeConstraintInfo objects in this branch of the tree.
 */
class MMPDispatcher : public Dispatcher<MMPAvailabilityInformation> {
public:
    /**
     * Constructs a QueueBalancingDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    MMPDispatcher(OverlayBranch & b) : Dispatcher(b) {}

    // This is documented in Dispatcher.
    virtual void recomputeInfo();

    static void setBeta(double b) { beta = b; }

    boost::shared_ptr<MMPAvailabilityInformation> getChildWaitingInfo(int c) const {
        return child[c].waitingInfo;
    }

private:
    static double beta;

    struct DecissionInfo;

    // This is documented in Dispatcher.
    virtual void handle(const CommAddress & src, const TaskBagMsg & msg);

    boost::shared_ptr<MMPAvailabilityInformation> getChildInfo(int c);
};

#endif /*MMPDISPATCHER_H_*/
