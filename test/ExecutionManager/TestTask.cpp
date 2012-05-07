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

#include "TestTask.hpp"
#include "Scheduler.hpp"

class TestExecutionEnvironment : public Scheduler::ExecutionEnvironment {
public:
	double getAveragePower() const { return 1000.0; }

	unsigned long int getAvailableMemory() const { return 1024; }

	unsigned long int getAvailableDisk() const { return 30000; }

	boost::shared_ptr<Task> createTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d) const {
		return boost::shared_ptr<Task>(new TestTask(o, reqId, ctid, d, getAveragePower()));
	}
};

Scheduler::ExecutionEnvironmentImpl::ExecutionEnvironmentImpl() : impl(new TestExecutionEnvironment) {}
