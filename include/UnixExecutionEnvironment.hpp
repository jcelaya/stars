/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya, María Ángeles Giménez
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

#ifndef UNIXEXECUTIONENVIRONMENT_H_
#define UNIXEXECUTIONENVIRONMENT_H_

#include <boost/shared_ptr.hpp>
#include "Scheduler.hpp"


class UnixExecutionEnvironment : public Scheduler::ExecutionEnvironment {
public:
    /**
     * This is described in Scheduler::ExecutionEnvironment
     */
    double getAveragePower() const;

    /**
     * This is described in Scheduler::ExecutionEnvironment
     */
    unsigned long int getAvailableMemory() const;

    /**
     * This is described in Scheduler::ExecutionEnvironment
     */
    unsigned long int getAvailableDisk() const;

    /**
     * This is described in Scheduler::ExecutionEnvironment
     */
    boost::shared_ptr<Task> createTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d) const;
};

#endif /* UNIXEXECUTIONENVIRONMENT_H_ */
