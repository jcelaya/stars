/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2011 Javier Celaya
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
