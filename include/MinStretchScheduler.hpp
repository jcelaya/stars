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

#ifndef MINSTRETCHSCHEDULER_H_
#define MINSTRETCHSCHEDULER_H_

#include "Scheduler.hpp"
#include "StretchInformation.hpp"


/**
 * \brief A fair scheduler that provides similar stretch to every application.
 *
 * This scheduler calculates the minimum stretch that can be assigned to all applications,
 * by calculating the deadlines so that they are all met.
 */
class MinStretchScheduler: public Scheduler {
public:
	/**
	 * Creates a new MinStretchScheduler with an empty list and an empty availability function.
	 * @param resourceNode Resource node associated with this scheduler.
	 */
	MinStretchScheduler(ResourceNode & resourceNode) : Scheduler(resourceNode) {
		reschedule();
		// The emptiness must also be notified.
		notifySchedule();
	}

	/**
	 * Generates a list of applications from a task queue, sorted so that they have the minimum stretch.
	 * @param tasks The task queue.
	 * @param result The resulting list of applications.
	 * @return The maximum stretch among all these applications, once they are ordered to minimize this value.
	 */
	static double sortMinStretch(const std::list<boost::shared_ptr<Task> > & tasks, std::list<StretchInformation::AppDesc> & result);

	// This is documented in Scheduler
	virtual unsigned int accept(const TaskBagMsg & msg);

	// This is documented in Scheduler
	virtual const StretchInformation & getAvailability() const { return info; }

private:
	StretchInformation info;

	// This is documented in Scheduler
	virtual void reschedule();
};

#endif /* MINSTRETCHSCHEDULER_H_ */
