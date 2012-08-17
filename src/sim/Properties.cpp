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

#include "Properties.hpp"
namespace fs = boost::filesystem;


std::ostream & operator<<(std::ostream & os, const Properties & o) {
    for (Properties::const_iterator it = o.begin(); it != o.end(); it++) {
        os << it->first << "=" << it->second << " ";
    }
    return os;
}


void Properties::loadFromFile(const fs::path & fileName) {
    fs::ifstream ifs(fileName);
    std::string nextLine;

    while (ifs.good()) {
        // Read next line
        std::getline(ifs, nextLine);
        // If it is just an empty line, skip it
        if (nextLine.length() == 0) continue;
        // If it is a comment, skip it
        if (nextLine[0] == '#') continue;

        // Get key
        size_t equalPos = nextLine.find_first_of('=');
        if (equalPos == std::string::npos) continue; // wrong format
        // Get Value
        // BEWARE!! Spaces are not ignored, as they can be part of valid values
        (*this)[nextLine.substr(0, equalPos)] = nextLine.substr(equalPos + 1);
    }
}
