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

#ifndef SIMPLESCHEDULER_H_
#define SIMPLESCHEDULER_H_

#include "Scheduler.hpp"
#include "BasicAvailabilityInfo.hpp"


/**
 * \brief Simple policy scheduler.
 *
 * This Scheduler subclass implements a task queue with only one slot and no deadline check at all.
 * It only reports if it is free or not.
 */
class SimpleScheduler : public Scheduler {
    BasicAvailabilityInfo info;

    // This is documented in Scheduler
    void reschedule();

    // This is documented in Scheduler
    unsigned int accept(const TaskBagMsg & msg);

public:
    /**
     * Creates a new SimpleScheduler with an empty task.
     * @param resourceNode Resource node associated with this scheduler.
     */
    SimpleScheduler(ResourceNode & resourceNode) : Scheduler(resourceNode) {
        reschedule();
    }

    // This is documented in Scheduler
    virtual const BasicAvailabilityInfo & getAvailability() const {
        return info;
    }
};

#endif /*SIMPLESCHEDULER_H_*/
