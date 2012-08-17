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

#include <sys/user.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include "MemoryManager.hpp"


MemoryManager::MemoryManager() : current(0), maxUsed(0), max(0), pagesize(sysconf(_SC_PAGESIZE)),
        pid(getpid()), nextUpdate(boost::posix_time::microsec_clock::local_time() - boost::posix_time::seconds(1)) {
    p = &pidText[14];
    strcpy(p--, "/stat");
    while (pid > 0) {
        *p-- = (pid % 10) + '0';
        pid /= 10;
    }
    p -= 5;
    memcpy(p, "/proc/", 6);
}


void MemoryManager::update() {
    boost::mutex::scoped_lock lock(m);
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
    if (now >= nextUpdate) {
        std::string dummy;
        std::ifstream meminfo("/proc/meminfo");
        meminfo >> dummy >> max;
        max = (max << 1) * 461;   // Approx 90% of total memory

        std::ifstream file(p);
        for (int i = 0; file.good() && i < 23; ++i)
            file.ignore(1000, ' ');
        file >> current;
        current *= pagesize;
        if (maxUsed < current) maxUsed = current;

        nextUpdate = now + boost::posix_time::seconds(5);
    }
}
