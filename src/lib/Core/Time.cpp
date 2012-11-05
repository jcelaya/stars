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

#include <boost/date_time/gregorian/gregorian.hpp>
#include "Time.hpp"
namespace pt = boost::posix_time;

static const pt::ptime & getReferenceTime() {
    static pt::ptime ref = pt::ptime(boost::gregorian::date(2000, boost::gregorian::Jan, 1), pt::seconds(0));
    return ref;
}


pt::ptime Time::to_posix_time() const {
    return getReferenceTime() + pt::microseconds(t);
}


void Time::from_posix_time(pt::ptime time) {
    if (time != pt::not_a_date_time)
        t = (time - getReferenceTime()).total_microseconds();
}


Time Time::getCurrentTime() {
    return Time((pt::microsec_clock::universal_time() - getReferenceTime()).total_microseconds());
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
    uint64_t secs = floor(val / 1000000);
    unsigned int days = floor(secs / 86400);
    secs -= days * 86400;
    unsigned int hours = floor(secs / 3600);
    secs -= hours * 3600;
    unsigned int minutes = floor(secs / 60);
    secs -= minutes * 60;
    return os << days
           << ':' << std::setw(2) << std::setfill('0') << hours
           << ':' << std::setw(2) << std::setfill('0') << minutes
           << ':' << std::setw(2) << std::setfill('0') << secs
           << '.' << std::setw(6) << std::setfill('0') << microsec << std::setfill(' ');
}


std::ostream & operator<<(std::ostream & os, const Time & r) {
    return os << r.to_posix_time();
}
