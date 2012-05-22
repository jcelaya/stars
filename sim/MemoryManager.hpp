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

#ifndef MEMORYMANAGER_H_
#define MEMORYMANAGER_H_

#include <sys/types.h>


class MemoryManager {
    unsigned long int max;
    long int pagesize;
    pid_t pid;
    char pidText[20], * p;
    
    MemoryManager();

public:
    static MemoryManager & getInstance() {
        static MemoryManager instance;
        return instance;
    }

    unsigned long int getMaxMemory();

    unsigned long int getUsedMemory();

    unsigned long int getMaxUsedMemory() {
        getUsedMemory();
        return max;
    }

    void reset() {
        max = 0;
    }
};

#endif /*MEMORYMANAGER_H_*/
