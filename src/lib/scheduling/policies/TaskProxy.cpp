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

#include "TaskProxy.hpp"

namespace stars {

void TaskProxy::List::sortBySlowness(double slowness) {
    if (size() > 1) {
        // Sort the vector, leaving the first task as is
        List tmp;
        tmp.splice(tmp.begin(), *this, begin());
        for (iterator i = begin(); i != end(); ++i)
            i->setSlowness(slowness);
        sort();
        splice(begin(), tmp, tmp.begin());
    }
}


bool TaskProxy::List::meetDeadlines(double slowness, Time e) const {
    for (const_iterator i = begin(); i != end(); ++i)
        if ((e += Duration(i->t)) > i->getDeadline(slowness))
            return false;
    return true;
}


void TaskProxy::List::sortMinSlowness(const std::vector<double> & switchValues) {
    Time now = Time::getCurrentTime();
    // Trivial case, first switch value is minimum slowness so that first task meets deadline
    if (switchValues.size() == 1) {
        sortBySlowness(switchValues.front() + 1.0);
        return;
    }
    // Calculate interval by binary search
    unsigned int minLi = 0, maxLi = switchValues.size() - 1;
    while (maxLi > minLi + 1) {
        unsigned int medLi = (minLi + maxLi) >> 1;
        sortBySlowness((switchValues[medLi] + switchValues[medLi + 1]) / 2.0);
        // For each app, check whether it is going to finish in time or not.
        if (meetDeadlines(switchValues[medLi], now))
            maxLi = medLi;
        else
            minLi = medLi;
    }
    // Sort them one last time
    sortBySlowness((switchValues[minLi] + switchValues[minLi + 1]) / 2.0);
    // If maxLi is still size-1, check interval lBounds[maxLi]-infinite
    if (maxLi == switchValues.size() - 1 && !meetDeadlines(switchValues.back(), now))
        sortBySlowness(switchValues.back() + 1.0);
}


void TaskProxy::List::getSwitchValues(std::vector<double> & switchValues) const {
    switchValues.clear();
    if (!empty()) {
        // Minimum switch value is first task slowness
        Time firstTaskEndTime = Time::getCurrentTime() + Duration(front().t);
        switchValues.push_back((firstTaskEndTime - front().rabs).seconds() / front().a);
        // Calculate bounds with the rest of the tasks, except the first
        for (const_iterator it = ++begin(); it != end(); ++it) {
            for (const_iterator jt = it; jt != end(); ++jt) {
                if (it->a != jt->a) {
                    double l = (jt->rabs - it->rabs).seconds() / (it->a - jt->a);
                    if (l > switchValues.front()) {
                        switchValues.push_back(l);
                    }
                }
            }
        }
        std::sort(switchValues.begin(), switchValues.end());
        // Remove duplicate values
        switchValues.erase(std::unique(switchValues.begin(), switchValues.end()), switchValues.end());
    }
}


double TaskProxy::List::getSlowness() const {
    double minSlowness = 0.0;
    Time e = Time::getCurrentTime();
    // For each task, calculate finishing time
    for (const_iterator i = begin(); i != end(); ++i) {
        e += Duration(i->t);
        double slowness = (e - i->rabs).seconds() / i->a;
        if (slowness > minSlowness)
            minSlowness = slowness;
    }

    return minSlowness;
}

} // namespace stars
