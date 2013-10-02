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
        unsigned long int totalNumEvents, partialNumEvents;
        double totalHandleTime, partialHandleTime;
        EventStats() : totalNumEvents(0), partialNumEvents(0), totalHandleTime(0.0), partialHandleTime(0.0) {}
    };

private:
    std::map<std::string, EventStats> handleTimeStatistics;
    fs::ofstream os;
    pt::time_duration lastPartialSave;

public:
    void openFile(const fs::path & statDir);

    void startEvent(const std::string & ev);

    void endEvent(const std::string & ev);

    EventStats getEvent(const std::string & ev) const;

    void savePartialStatistics();

    void saveTotalStatistics();
};

#endif /* PERFORMANCESTATISTICS_H_ */
