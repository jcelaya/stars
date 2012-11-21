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

#ifndef MSPSCHEDULER_H_
#define MSPSCHEDULER_H_

#include <vector>
#include "Scheduler.hpp"
#include "MSPAvailabilityInformation.hpp"


/**
 * \brief A fair scheduler that provides similar stretch to every application.
 *
 * This scheduler calculates the minimum stretch that can be assigned to all applications,
 * by calculating the deadlines so that they are all met.
 */
class MSPScheduler: public Scheduler {
public:
    /**
     * Creates a new MinStretchScheduler with an empty list and an empty availability function.
     * @param resourceNode Resource node associated with this scheduler.
     */
    MSPScheduler(ResourceNode & resourceNode) : Scheduler(resourceNode) {
        reschedule();
        notifySchedule();
    }

    /**
     * Generates a list of applications from a task queue, sorted so that they have the minimum stretch.
     * @param tasks The task queue.
     * @param result The resulting list of applications.
     * @return The maximum stretch among all these applications, once they are ordered to minimize this value.
     */
    static double sortMinSlowness(std::list<TaskProxy> & proxys, const std::vector<double> & lBounds, std::list<boost::shared_ptr<Task> > & tasks);

    // This is documented in Scheduler
    virtual unsigned int acceptable(const TaskBagMsg & msg);

    // This is documented in Scheduler
    virtual const MSPAvailabilityInformation & getAvailability() const {
        return info;
    }

private:
    MSPAvailabilityInformation info;
    std::list<TaskProxy> proxys;
    std::vector<std::pair<double, int> > switchValues;

    // This is documented in Scheduler
    virtual void reschedule();

    // This is documented in Scheduler
    virtual void removeTask(const boost::shared_ptr<Task> & task);

    // This is documented in Scheduler
    virtual void acceptTask(const boost::shared_ptr<Task> & task);
};

#endif /* MSPSCHEDULER_H_ */
