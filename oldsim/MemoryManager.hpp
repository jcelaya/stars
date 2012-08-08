/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
 *
 *  This file is part of STaRS.
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

    void reset() {
        max = maxUsed = current = 0;
    }
};

#endif /*MEMORYMANAGER_H_*/
