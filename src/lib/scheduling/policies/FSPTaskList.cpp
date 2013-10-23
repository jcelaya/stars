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

bool FSPTaskList::preemptive = true;


void FSPTaskList::addBoundaryValues(const TaskProxy& task, std::vector<double> & altBoundaries) {
    for (auto it = preemptive ? begin() : ++begin(); it != end(); ++it)
        if (it->a != task.a) {
            double l = (task.rabs - it->rabs).seconds() / (it->a - task.a);
            if (l > 0.0) {
                altBoundaries.push_back(l);
            }
        }
    std::sort(altBoundaries.begin(), altBoundaries.end());
    // Remove duplicate values
    altBoundaries.erase(std::unique(altBoundaries.begin(), altBoundaries.end()), altBoundaries.end());
}


void FSPTaskList::addTasks(const TaskProxy & task, unsigned int n) {
    // Calculate bounds with the rest of the tasks, except the first
    if (!empty()) {
        addBoundaryValues(task, boundaries);
    } else if (!preemptive) {
        // Minimum switch value is first task slowness
        Time firstTaskEndTime = Time::getCurrentTime() + Duration(task.t);
        boundaries.push_back((firstTaskEndTime - task.rabs).seconds() / task.a);
    }
    insert(end(), n, task);
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
        std::list<TaskProxy> tmp;
        if (!preemptive)
            tmp.splice(tmp.begin(), *this, begin());
        for (iterator i = begin(); i != end(); ++i)
            i->setSlowness(slowness);
        sort();
        if (!preemptive)
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
            if (!preemptive) {
                // Minimum switch value is first task slowness
                Time firstTaskEndTime = Time::getCurrentTime() + Duration(front().t);
                boundaries.push_back((firstTaskEndTime - front().rabs).seconds() / front().a);
            }
            // Calculate bounds with the rest of the tasks, except the first
            for (auto it = preemptive ? begin() : ++begin(); it != end(); ++it) {
                for (auto jt = it; jt != end(); ++jt) {
                    if (it->a != jt->a) {
                        double l = (jt->rabs - it->rabs).seconds() / (it->a - jt->a);
                        if (l > 0.0) {
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
    if (!empty()) {
        Time now = Time::getCurrentTime();
        if (altBoundaries.empty()) {
            sortBySlowness(1.0);
        } else if (altBoundaries.size() == 1) {
            sortBySlowness(altBoundaries.front() / 2.0);
            if (!meetDeadlines(altBoundaries.front(), now))
                sortBySlowness(altBoundaries.front() + 1.0);
        } else {
            unsigned int minLi = 0, maxLi = altBoundaries.size() - 1;
            // Calculate interval by binary search
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
    }
}

void FSPTaskList::updateReleaseTime() {
    Time now = Time::getCurrentTime();
    for (auto & i: *this)
        i.r = (i.rabs - now).seconds();
}


} // namespace stars
