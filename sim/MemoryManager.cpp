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

#include "MemoryManager.hpp"
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
using namespace std;


unsigned long int MemoryManager::getMaxMemory() {
    unsigned long int result;
    string dummy;
    ifstream file("/proc/meminfo");
    file >> dummy >> result;
    return (result << 1) * 461;   // Approx 90% of total memory
}


unsigned long int MemoryManager::getUsedMemory() {
    unsigned long int result = 0;
    pid_t pid = getpid();
    char pidText[20], * p = &pidText[14];
    strcpy(p--, "/stat");
    while (pid > 0) {
        *p-- = (pid % 10) + '0';
        pid /= 10;
    }
    p -= 5;
    memcpy(p, "/proc/", 6);
    ifstream file(p);
    if (file.good()) {
        for (int i = 0; i < 23; i++)
            file.ignore(1000, ' ');
        file >> result;
    }
    result *= sysconf(_SC_PAGESIZE);
    if (max < result) max = result;
    return result;
}
