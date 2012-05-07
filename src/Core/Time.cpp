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

#include <boost/date_time/gregorian/gregorian.hpp>
#include "Time.hpp"
using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::gregorian;

static const ptime & getReferenceTime() {
	static ptime ref = ptime(date(2000, Jan, 1), seconds(0));
	return ref;
}


ptime Time::to_posix_time() const {
	return getReferenceTime() + microseconds(t);
}


void Time::from_posix_time(ptime time) {
	if (time != not_a_date_time)
		t = (time - getReferenceTime()).total_microseconds();
}


Time Time::getCurrentTime() {
	return Time((microsec_clock::universal_time() - getReferenceTime()).total_microseconds());
}


std::ostream & operator<<(std::ostream & os, const Duration & r) {
	bool negative = r.d < 0;
	uint64_t val;
	if (negative) {
		val = -r.d;
		os << '-';
	}
	else val = r.d;
	unsigned int microsec = val % 1000000;
	uint64_t tt = floor(val / 1000000);
	unsigned int hours = floor(tt / 3600);
	unsigned int minutes = floor((tt - hours * 3600) / 60);
	unsigned int secs = tt - hours * 3600 - minutes * 60;
	return os << hours
		<< ':' << std::setw(2) << std::setfill('0') << minutes
		<< ':' << std::setw(2) << std::setfill('0') << secs
		<< '.' << std::setw(6) << std::setfill('0') << microsec << std::setfill(' ');
}


std::ostream & operator<<(std::ostream & os, const Time & r) {
	return os << r.to_posix_time();
}
