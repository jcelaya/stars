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

#ifndef IBPDISPATCHER_H_
#define IBPDISPATCHER_H_

#include "Dispatcher.hpp"
#include "IBPAvailabilityInformation.hpp"


class IBPDispatcher: public Dispatcher<IBPAvailabilityInformation> {
    struct DecisionInfo;

    /**
     * A Task bag allocation request. It is received when a client wants to assign a group
     * of task to a set of available execution nodes.
     * @param src Source node address.
     * @param msg TaskBagMsg message with task group information.
     */
    void handle(const CommAddress & src, const TaskBagMsg & msg);

public:
    /**
     * Constructs a DeadlineDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    IBPDispatcher(StructureNode & sn) : Dispatcher(sn) {}

    // This is documented in Dispatcher.
    virtual void recomputeInfo();
};

#endif /* IBPDISPATCHER_H_ */
