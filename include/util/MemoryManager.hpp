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

#ifndef MEMORYMANAGER_H_
#define MEMORYMANAGER_H_

#include <sys/types.h>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/ptime.hpp>


class MemoryManager {
    unsigned long int current, maxUsed, max;
    long int pagesize;
    pid_t pid;
    char pidText[20], * p;
    boost::posix_time::ptime nextUpdate;
    boost::posix_time::time_duration updateDuration;
    boost::mutex m;

    MemoryManager();

    void update();

public:
    static MemoryManager & getInstance() {
        static MemoryManager instance;
        return instance;
    }

    unsigned long int getMaxMemory() {
        update();
        return max;
    }

    unsigned long int getUsedMemory() {
        update();
        return current;
    }

    unsigned long int getMaxUsedMemory() {
        update();
        return maxUsed;
    }

    void setUpdateDuration(double seconds) {
        updateDuration = boost::posix_time::microseconds(seconds * 1000000.0);
    }

    void reset() {
        max = maxUsed = current = 0;
    }
};

#endif /*MEMORYMANAGER_H_*/
