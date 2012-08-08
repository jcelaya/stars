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

#include "Properties.hpp"
using namespace std;


ostream & operator<<(ostream & os, const Properties & o) {
	for (Properties::const_iterator it = o.begin(); it != o.end(); it++) {
		os << it->first << "=" << it->second << " ";
	}
	return os;
}


void Properties::loadFromFile(const fs::path & fileName) {
	fs::ifstream ifs(fileName);
	string nextLine;

	while (ifs.good()) {
		// Read next line
		getline(ifs, nextLine);
		// If it is just an empty line, skip it
		if (nextLine.length() == 0) continue;
		// If it is a comment, skip it
		if (nextLine[0] == '#') continue;

		// Get key
		size_t equalPos = nextLine.find_first_of('=');
		if (equalPos == string::npos) continue; // wrong format
		// Get Value
		// BEWARE!! Spaces are not ignored, as they can be part of valid values
		(*this)[nextLine.substr(0, equalPos)] = nextLine.substr(equalPos + 1);
	}
}
