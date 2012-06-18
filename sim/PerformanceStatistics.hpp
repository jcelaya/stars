/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
 *
 *  This file is part of STaRS.
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

#ifndef PERFORMANCESTATISTICS_H_
#define PERFORMANCESTATISTICS_H_

#include <vector>
#include <map>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread/mutex.hpp>


/**
 * A record of the time needed by each type of event to finish.
 */
class PerformanceStatistics {
public:
    /**
     * A record for a certain kind of event.
     */
    struct EventStats {
        unsigned long int totalNumEvents;     ///< Total number of events throughout the simulation.
        unsigned long int partialNumEvents;   ///< Number of events recorded since the last report.
        double totalHandleTime;               ///< Total time spent in this kind of event.
        double partialHandleTime;             ///< Total time spent in this kind of event since the last report.
        /// Default constructor, resets statistics.
        EventStats() : totalNumEvents(0), partialNumEvents(0), totalHandleTime(0.0), partialHandleTime(0.0) {}
    };

    void resizeNumNodes(size_t n) {
        start.resize(n);
    }
    
    /// Open the statistics file at the given directory.
    void openStatsFile();
    
    /**
     * Start measuring a certain kind of event.
     * @param ev Event name.
     */
    void startEvent(const std::string & ev);
    
    /**
     * Finish measuring a certain kind of event.
     * @param ev Event name.
     */
    void endEvent(const std::string & ev);
    
    /**
     * Obtain the recorded statistics for a certain kind of event.
     * @param ev Event name.
     */
    EventStats getEvent(const std::string & ev) const;
    
    /**
     * Save a partial report of the performance statistics.
     */
    void savePartialStatistics();
    
    /**
     * Save all the statistics to file.
     */
    void saveTotalStatistics();

private:
    /// Last measurement starting time per node and event type
    std::vector<std::map<std::string, boost::posix_time::ptime> > start;
    std::map<std::string, EventStats> handleTimeStatistics;   ///< Association between event names and statistics.
    boost::filesystem::ofstream os;                           ///< Output file.
    boost::mutex m;                                           ///< Access mutex.
};

#endif /* PERFORMANCESTATISTICS_H_ */
