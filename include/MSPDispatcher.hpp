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

#ifndef MSPDISPATCHER_H_
#define MSPDISPATCHER_H_

#include "Dispatcher.hpp"
#include "MSPAvailabilityInformation.hpp"
#include "Time.hpp"


/**
 * \brief Resource request dispatcher for execution node requests.
 *
 * This Dispatcher receives TaskBagMsg requests to assign tasks to execution nodes. It also
 * controls the aggregation of TimeConstraintInfo objects in this branch of the tree.
 */
class MSPDispatcher : public Dispatcher<MSPAvailabilityInformation> {
public:
    /**
     * Constructs a QueueBalancingDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    MSPDispatcher(StructureNode & sn) : Dispatcher(sn) {}

    // This is documented in Dispatcher.
    virtual void recomputeInfo();

private:
    // This is documented in Dispatcher.
    virtual void handle(const CommAddress & src, const TaskBagMsg & msg);
};

#endif /* MSPDISPATCHER_H_ */
