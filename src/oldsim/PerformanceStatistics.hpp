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

#ifndef PERFORMANCESTATISTICS_H_
#define PERFORMANCESTATISTICS_H_

#include <map>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
namespace pt = boost::posix_time;

class PerformanceStatistics {
public:
    struct EventStats {
        pt::ptime start;
        unsigned long int numEvents;
        unsigned long int minDuration;
        unsigned long int maxDuration;
        double handleTime;
        void reset() {
            numEvents = minDuration = maxDuration = 0;
            handleTime = 0.0;
        }
        void end();
        EventStats & operator+=(const EventStats & r);
        EventStats() { reset(); }
    };

    void openFile(const fs::path & statDir);

    void startEvent(const std::string & ev);

    void endEvent(const std::string & ev);

    void savePartialStatistics();

    void saveTotalStatistics();

private:
    std::map<std::string, EventStats> partialStatsPerEvent;
    std::map<std::string, EventStats> statsPerEvent;
    fs::ofstream os;
    pt::time_duration lastPartialSave;

    void copyPartialToTotal();
};

#endif /* PERFORMANCESTATISTICS_H_ */
