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

#ifndef EDFSCHEDULER_H_
#define EDFSCHEDULER_H_

#include "Scheduler.hpp"
#include "TimeConstraintInfo.hpp"


/**
 * \brief Scheduler with an EDF scheduling policy.
 *
 * This class is a Scheduler implementation with an EDF ordering policy. The tasks are ordered
 * by its deadline, so that tasks with earlier deadline are executed first.
 */
class EDFScheduler : public Scheduler {
    TimeConstraintInfo info;   ///< Current availability function

    /**
     * Calculates the availability function from the list of tasks,
     * and updates the availability summary of Scheduler
     */
    void calculateAvailability();

    // This is documented in Scheduler
    void reschedule();

public:
    /**
     * Creates a new EDFScheduler with an empty list and an empty availability function.
     * @param resourceNode Resource node associated with this scheduler.
     */
    EDFScheduler(ResourceNode & resourceNode) : Scheduler(resourceNode) {
        calculateAvailability();
    }

    // This is documented in Scheduler
    unsigned int acceptable(const TaskBagMsg & msg);

    /// Returns the availability before certain deadline.
    unsigned long int getAvailabilityBefore(Time d);

    // This is documented in Scheduler
    virtual const TimeConstraintInfo & getAvailability() const {
        return info;
    }
};

#endif /*EDFSCHEDULER_H_*/
