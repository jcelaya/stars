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

#ifndef FSPTASKLIST_HPP_
#define FSPTASKLIST_HPP_

#include <list>
#include <vector>
#include "TaskProxy.hpp"
#include "Time.hpp"

namespace stars {

class FSPTaskList : public std::list<TaskProxy> {
public:
    FSPTaskList() : std::list<TaskProxy>(), dirty(false) {}

    explicit FSPTaskList(std::list<TaskProxy> && copy) : std::list<TaskProxy>(copy), dirty(true) {}

    void addTasks(const TaskProxy & task, unsigned int n = 1);

    void removeTask(unsigned int id);

    void sortMinSlowness() {
        computeBoundaries();
        sortMinSlowness(boundaries);
    }

    void sortMinSlowness(const std::vector<double> & altBoundaries);

    void sortBySlowness(double slowness);

    double getSlowness() const;

    bool meetDeadlines(double slowness, Time e) const;

    const std::vector<double> & getBoundaries() {
        computeBoundaries();
        return boundaries;
    }

    void updateReleaseTime();

private:
    void computeBoundaries();

    std::vector<double> boundaries;
    bool dirty;
};

} // namespace stars

#endif /* FSPTASKLIST_HPP_ */
