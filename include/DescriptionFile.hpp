/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya, María Ángeles Giménez
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
