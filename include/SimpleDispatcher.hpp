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

#ifndef SIMPLEDISPATCHER_H_
#define SIMPLEDISPATCHER_H_

#include "Dispatcher.hpp"
#include "BasicAvailabilityInfo.hpp"


class SimpleDispatcher: public Dispatcher<BasicAvailabilityInfo> {
    struct DecisionInfo;

    /**
     * A Task bag allocation request. It is received when a client wants to assign a group
     * of task to a set of available execution nodes.
     * @param src Source node address.
     * @param msg TaskBagMsg message with task group information.
     */
    void handle(const CommAddress & src, const TaskBagMsg & msg);

    // This is documented in Dispatcher.
    virtual void recomputeInfo();

public:
    /**
     * Constructs a DeadlineDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    SimpleDispatcher(StructureNode & sn) : Dispatcher(sn) {}
};

#endif /* SIMPLEDISPATCHER_H_ */
