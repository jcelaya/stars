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

#include "FSPTaskList.hpp"

namespace stars {

void FSPTaskList::addTasks(const TaskProxy & task, unsigned int n) {
    // Calculate bounds with the rest of the tasks, except the first
    if (!empty()) {
        for (auto it = ++begin(); it != end(); ++it)
            if (it->a != task.a) {
                double l = (task.rabs - it->rabs).seconds() / (it->a - task.a);
                if (l > boundaries.front()) {
                    boundaries.push_back(l);
                }
            }
        std::sort(boundaries.begin(), boundaries.end());
        // Remove duplicate values
        boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
    } else {
        // Minimum switch value is first task slowness
        Time firstTaskEndTime = Time::getCurrentTime() + Duration(task.t);
        boundaries.push_back((firstTaskEndTime - task.rabs).seconds() / task.a);
    }
    insert(end(), n, task);
    //dirty = true;
}


void FSPTaskList::removeTask(unsigned int id) {
    // Look for the proxy
    for (auto p = begin(); p != end(); ++p) {
        if (p->id == id) {
            // Remove the proxy
            erase(p);
            dirty = true;
            break;
        }
    }
}


double FSPTaskList::getSlowness() const {
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


void FSPTaskList::sortBySlowness(double slowness) {
    if (size() > 1) {
        // Sort the vector, leaving the first task as is
        std::list<TaskProxy> tmp;
        tmp.splice(tmp.begin(), *this, begin());
        for (iterator i = begin(); i != end(); ++i)
            i->setSlowness(slowness);
        sort();
        splice(begin(), tmp, tmp.begin());
    }
}


bool FSPTaskList::meetDeadlines(double slowness, Time e) const {
    for (const_iterator i = begin(); i != end(); ++i)
        if ((e += Duration(i->t)) > i->getDeadline(slowness))
            return false;
    return true;
}


void FSPTaskList::computeBoundaries() {
    if (dirty) {
        dirty = false;
        boundaries.clear();
        if (!empty()) {
            // Minimum switch value is first task slowness
            Time firstTaskEndTime = Time::getCurrentTime() + Duration(front().t);
            boundaries.push_back((firstTaskEndTime - front().rabs).seconds() / front().a);
            // Calculate bounds with the rest of the tasks, except the first
            for (const_iterator it = ++begin(); it != end(); ++it) {
                for (const_iterator jt = it; jt != end(); ++jt) {
                    if (it->a != jt->a) {
                        double l = (jt->rabs - it->rabs).seconds() / (it->a - jt->a);
                        if (l > boundaries.front()) {
                            boundaries.push_back(l);
                        }
                    }
                }
            }
            std::sort(boundaries.begin(), boundaries.end());
            // Remove duplicate values
            boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
        }
    }
}


void FSPTaskList::sortMinSlowness(const std::vector<double> & altBoundaries) {
    if (empty()) return;
    Time now = Time::getCurrentTime();
    // Trivial case, first switch value is minimum slowness so that first task meets deadline
    if (altBoundaries.size() == 1) {
        sortBySlowness(altBoundaries.front() + 1.0);
        return;
    }
    // Calculate interval by binary search
    unsigned int minLi = 0, maxLi = altBoundaries.size() - 1;
    while (maxLi > minLi + 1) {
        unsigned int medLi = (minLi + maxLi) >> 1;
        sortBySlowness((altBoundaries[medLi] + altBoundaries[medLi + 1]) / 2.0);
        // For each app, check whether it is going to finish in time or not.
        if (meetDeadlines(altBoundaries[medLi], now))
            maxLi = medLi;
        else
            minLi = medLi;
    }
    // Sort them one last time
    sortBySlowness((altBoundaries[minLi] + altBoundaries[minLi + 1]) / 2.0);
    // If maxLi is still size-1, check interval lBounds[maxLi]-infinite
    if (maxLi == altBoundaries.size() - 1 && !meetDeadlines(altBoundaries.back(), now))
        sortBySlowness(altBoundaries.back() + 1.0);
}

void FSPTaskList::updateReleaseTime() {
    Time now = Time::getCurrentTime();
    for (auto i = begin(); i != end(); ++i)
        i->r = (i->rabs - now).seconds();
}


} // namespace stars
