/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2008 María Ángeles Giménez
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

#ifndef DESCRIPTIONFILE_H_
#define DESCRIPTIONFILE_H_

#include <string>


class DescriptionFile {
    std::string executable;
    std::string result;
    std::string length;
    std::string memory;
    std::string disk;

public:
    DescriptionFile(std::string taskName);
    ~DescriptionFile() {}

    std::string getExecutable() {
        return executable;
    }
    std::string getResult() {
        return result;
    }
    std::string getLength() {
        return length;
    }
    std::string getMemory() {
        return memory;
    }
    std::string getDisk() {
        return disk;
    }

};

#endif /*DESCRIPTIONFILE_H_*/
