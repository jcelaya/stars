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

#ifndef IBPCHEDULER_H_
#define IBPCHEDULER_H_

#include "Scheduler.hpp"
#include "IBPAvailabilityInformation.hpp"


/**
 * \brief Simple policy scheduler.
 *
 * This Scheduler subclass implements a task queue with only one slot and no deadline check at all.
 * It only reports if it is free or not.
 */
class IBPScheduler : public Scheduler {
    // This is documented in Scheduler
    void reschedule();

    // This is documented in Scheduler
    unsigned int acceptable(const TaskBagMsg & msg);

public:
    /**
     * Creates a new SimpleScheduler with an empty task queue.
     * @param resourceNode Resource node associated with this scheduler.
     */
    IBPScheduler(OverlayLeaf & l) : Scheduler(l) {
        reschedule();
        notifySchedule();
    }

    // This is documented in Scheduler
    virtual IBPAvailabilityInformation * getAvailability() const;
};

#endif /* IBPCHEDULER_H_ */
