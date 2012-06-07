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

#ifndef MINSLOWNESSDISPATCHER_H_
#define MINSLOWNESSDISPATCHER_H_

#include "Dispatcher.hpp"
#include "SlownessInformation.hpp"
#include "Time.hpp"


/**
 * \brief Resource request dispatcher for execution node requests.
 *
 * This Dispatcher receives TaskBagMsg requests to assign tasks to execution nodes. It also
 * controls the aggregation of TimeConstraintInfo objects in this branch of the tree.
 */
class MinSlownessDispatcher : public Dispatcher<SlownessInformation> {
public:
    /**
     * Constructs a QueueBalancingDispatcher and associates it with the corresponding StructureNode.
     * @param sn The StructureNode of this branch.
     */
    MinSlownessDispatcher(StructureNode & sn) : Dispatcher(sn) {}
    
    // This is documented in Dispatcher.
    virtual void recomputeInfo();
    
private:
    // This is documented in Dispatcher.
    virtual void handle(const CommAddress & src, const TaskBagMsg & msg);
};

#endif /* MINSLOWNESSDISPATCHER_H_ */
