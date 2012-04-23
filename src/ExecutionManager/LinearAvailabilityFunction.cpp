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

#include "LinearAvailabilityFunction.hpp"
#include "Time.hpp"
#include <cmath>
using namespace std;


unsigned long int LinearAvailabilityFunction::operator()(Time deadline) const {
	list<AvailPair>::const_iterator it;
	Time startSlope = Time::getCurrentTime();
	unsigned long int avail = 0;

	// Look for the reference point just after the deadline
	for (it = point.begin(); it != point.end() && it->first < deadline; it++) {
		startSlope = it->first;
		avail += it->second;
	}

	// Calculate availability with slope at deadline
	unsigned long int result = avail + (unsigned long int)floor(power * (deadline - startSlope).seconds());

	// If there is another point after deadline, return the lower availability
	if (it != point.end()) {
		avail += it->second;
		if (result > avail) result = avail;
	}

	return result;
}


void LinearAvailabilityFunction::addNewHole(Time p, unsigned long int a) {
	list<AvailPair>::iterator it;
	// Look for the reference point just after p
	for (it = point.begin(); it != point.end() && it->first < p; it++);
	// Remove it if it already exists
	if (it != point.end() && it->first == p)
		it->second = a;
	else
		point.insert(it, AvailPair(p, a));
}


ostream & operator<<(ostream & os, const LinearAvailabilityFunction & l) {
#ifdef _DEBUG_
	if (!l.point.empty()) {
		unsigned long int avail = 0;
		list<LinearAvailabilityFunction::AvailPair>::const_iterator it = l.point.begin(), prev = it++;
		Time now = Time::getCurrentTime();
		os << "(ref=" << now << ")";
		os << "(" << (prev->first - now).seconds() << ")";
		for (; it != l.point.end(); prev = it, it++) {
			avail += it->second;
			os << "(" << (prev->first - now).seconds() + it->second / l.power
				<< " -> " << (it->first - now).seconds() << ", " << avail << ")";
		}
	} else {
		os << "(free, " << l.power << ")";
	}
#endif // _DEBUG_
	return os;
}
