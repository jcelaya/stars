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

#ifndef TRAFFICSTATISTICS_H_
#define TRAFFICSTATISTICS_H_

#include <map>
#include <string>
#include <vector>
#include <list>
#include <ostream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Time.hpp"

class TrafficStatistics {

    struct MessageStats {
        unsigned long int numMessages;
        unsigned long int minSize, maxSize, totalBytes;
        MessageStats() : numMessages(0), minSize(100000000), maxSize(0), totalBytes(0) {}

        void addMessage(unsigned int size) {
            numMessages++;
            totalBytes += size;
            if (maxSize < size) maxSize = size;
            if (minSize > size) minSize = size;
        }

        friend std::ostream & operator<<(std::ostream & os, const MessageStats & o) {
            os << o.numMessages << ',' << o.totalBytes << ',';
            if (o.numMessages) {
                return os << o.minSize << ',' << o.maxSize;
            } else {
                return os << "n.a.,n.a.";
            }
        }
    };

    struct LevelStats {
        MessageStats sent, received;
    };

    std::vector<std::map<std::string, LevelStats> > typeStatsPerLevel;

public:
    void saveTotalStatistics();

    void msgReceivedAtLevel(unsigned int level, unsigned int size, const std::string & name);
    void msgSentAtLevel(unsigned int level, unsigned int size, const std::string & name);
};

#endif /*TRAFFICSTATISTICS_H_*/
