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

#ifndef MMPSCHEDULER_H_
#define MMPSCHEDULER_H_

#include "Scheduler.hpp"
#include "MMPAvailabilityInformation.hpp"


/**
 * \brief First Come First Served policy scheduler.
 *
 * This Scheduler subclass implements a task queue ordered by arrival time. A new task will be
 * inserted at the end, so it is very simple to check if it will finish in time or not.
 */
class MMPScheduler : public Scheduler {
    // This is documented in Scheduler
    void reschedule();

    // This is documented in Scheduler
    unsigned int acceptable(const TaskBagMsg & msg);

public:
    /**
     * Creates a new FCFSScheduler with an empty list and an empty AvailabilityFunction.
     * @param e Execution node associated with this scheduler.
     */
    MMPScheduler(OverlayLeaf & l) : Scheduler(l) {
        reschedule();
        notifySchedule();
    }

    // This is documented in Scheduler
    virtual MMPAvailabilityInformation * getAvailability() const;
};

#endif /* MMPSCHEDULER_H_ */
