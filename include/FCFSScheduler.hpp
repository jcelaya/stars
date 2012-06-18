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

#ifndef FCFSSCHEDULER_H_
#define FCFSSCHEDULER_H_

#include "Scheduler.hpp"
#include "QueueBalancingInfo.hpp"


/**
 * \brief First Come First Served policy scheduler.
 *
 * This Scheduler subclass implements a task queue ordered by arrival time. A new task will be
 * inserted at the end, so it is very simple to check if it will finish in time or not.
 */
class FCFSScheduler : public Scheduler {
    QueueBalancingInfo info;   ///< Current availability function

    // This is documented in Scheduler
    void reschedule();

    // This is documented in Scheduler
    unsigned int accept(const TaskBagMsg & msg);

public:
    /**
     * Creates a new FCFSScheduler with an empty list and an empty AvailabilityFunction.
     * @param e Execution node associated with this scheduler.
     */
    FCFSScheduler(ResourceNode & resourceNode) : Scheduler(resourceNode) {
        reschedule();
    }

    // This is documented in Scheduler
    virtual const QueueBalancingInfo & getAvailability() const {
        return info;
    }
};

#endif /*FCFSSCHEDULER_H_*/
