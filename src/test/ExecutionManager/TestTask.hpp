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

#ifndef TESTTASK_HPP_
#define TESTTASK_HPP_

#include "Task.hpp"


class TestTask : public Task {
    int status;
    Duration duration;

public:
    TestTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d, double power) :
            Task(o, reqId, ctid, d), status(Prepared), duration(description.getLength() / power) {}

    int getStatus() const {
        return status;
    }

    void run() {
        status = Running;
        // Send a finished message to the backend
    }

    void abort() {}

    Duration getEstimatedDuration() const {
        return duration;
    }
};

#endif /*TESTTASK_HPP_*/
