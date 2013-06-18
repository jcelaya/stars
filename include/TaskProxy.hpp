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

#ifndef TASKPROXY_HPP_
#define TASKPROXY_HPP_

#include <boost/shared_ptr.hpp>
#include "Task.hpp"
#include "Time.hpp"

namespace stars {

struct TaskProxy {
    boost::shared_ptr<Task> origin;
    unsigned int id;
    Time rabs, d;
    double a;
    double r, t, tsum;
    TaskProxy(double _a, double power, Time r) : id(-1), rabs(r), a(_a), r(0.0), t(a / power), tsum(t) {}
    TaskProxy(const boost::shared_ptr<Task> & task) : origin(task), id(task->getTaskId()), rabs(task->getCreationTime()),
            a(task->getDescription().getLength()), r(0.0), t(task->getEstimatedDuration().seconds()), tsum(t) {}
    Time getDeadline(double L) const { return rabs + Duration(L * a); }
    void setSlowness(double L) { d = getDeadline(L); }
    bool operator<(const TaskProxy & rapp) const { return d < rapp.d || (d == rapp.d && a < rapp.a); }
    double getEffectiveSpeed() { return a / (t + (Time::getCurrentTime() - rabs).seconds()); }
};

} // namespace stars

#endif /* TASKPROXY_HPP_ */
