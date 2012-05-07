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

#ifndef TESTTASK_HPP_
#define TESTTASK_HPP_

#include "Task.hpp"


class TestTask : public Task {
	int status;
	Duration duration;

public:
	TestTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d, double power) :
		Task(o, reqId, ctid, d), status(Prepared), duration(description.getLength() / power) {}

	int getStatus() const { return status; }

	void run() {
		status = Running;
		// Send a finished message to the backend
	}

	void abort() {}

	Duration getEstimatedDuration() const { return duration; }
};

#endif /*TESTTASK_HPP_*/
