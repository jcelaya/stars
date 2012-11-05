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

#include <limits>
#include <stdexcept>
#include "ZoneDescription.hpp"
using namespace std;


bool ZoneDescription::operator==(const ZoneDescription & r) const {
    return availableStrNodes == r.availableStrNodes && minAddr == r.minAddr &&
           maxAddr == r.maxAddr;
}

bool ZoneDescription::contains(const CommAddress & src) const {
    return minAddr <= src && src <= maxAddr;
}


double ZoneDescription::distance(const CommAddress & src) const {
    double result = 0.0;
    if (src < minAddr) result += src.distance(minAddr);
    else if (maxAddr < src) result += src.distance(maxAddr);
    return result;
}


double ZoneDescription::distance(const ZoneDescription & r) const {
    double result = 0.0; //info->distance(*r.info);
    if (r.maxAddr < maxAddr) {
        if (r.minAddr < minAddr) result += maxAddr.distance(r.minAddr); // Not overlapped or semi-overlapped
    } else { // The other case
        if (minAddr < r.minAddr) result += r.maxAddr.distance(minAddr); // Not overlapped or semi-overlapped
    }
    return result;
}


void ZoneDescription::aggregate(const std::list<boost::shared_ptr<ZoneDescription> > & zones) {
    // Pre: !zones.empty() && all zones non-null

    // Reset fields
    minAddr = zones.front()->minAddr;
    maxAddr = zones.front()->maxAddr;

    // Just sum up the available nodes in all the children
    int avail = 0;

    for (std::list<boost::shared_ptr<ZoneDescription> >::const_iterator it = zones.begin();
            it != zones.end(); it++) {
        // Calculate the resulting number of available Structure nodes
        avail += (*it)->getAvailableStrNodes();
        // Calculate the minimum address
        if ((*it)->minAddr < minAddr)
            minAddr = (*it)->minAddr;
        // Calculate the maximum address
        if (maxAddr < (*it)->maxAddr)
            maxAddr = (*it)->maxAddr;
    }

    // Now round up the result to the higher power of two lower than itself.
    // For example, if the result is 45, round it to 32.
    if (avail) {
        availableStrNodes = 1;
        while (avail != 1) {
            availableStrNodes <<= 1;
            avail >>= 1;
        }
    } else availableStrNodes = 0;
}


std::ostream& operator<<(std::ostream& os, const ZoneDescription & s) {
#ifdef _DEBUG_
    os << "{" << s.minAddr << "-" << s.maxAddr << "} a=" << s.availableStrNodes;
#endif // _DEBUG_
    return os;
}
