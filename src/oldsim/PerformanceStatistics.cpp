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

#include <list>
#include "PerformanceStatistics.hpp"
#include "Simulator.hpp"


void PerformanceStatistics::openFile(const fs::path & statDir) {
    os.open(statDir / fs::path("perf.stat"));
}


void PerformanceStatistics::startEvent(const std::string & ev) {
    handleTimeStatistics[ev].start = std::clock();
}


void PerformanceStatistics::endEvent(const std::string & ev) {
    clock_t end = std::clock();
    EventStats & es = handleTimeStatistics[ev];
    es.partialNumEvents++;
    es.totalNumEvents++;
    es.partialHandleTime += (end - es.start) / clocksPerUSecond;
    es.totalHandleTime += (end - es.start) / clocksPerUSecond;
}


PerformanceStatistics::EventStats PerformanceStatistics::getEvent(const std::string & ev) const {
    std::map<std::string, EventStats>::const_iterator it = handleTimeStatistics.find(ev);
    if (it != handleTimeStatistics.end())
        return it->second;
    else return EventStats();
}


struct TimePerEvent {
    double tpe;
    std::map<std::string, PerformanceStatistics::EventStats>::iterator ev;
    TimePerEvent(double t, std::map<std::string, PerformanceStatistics::EventStats>::iterator it) : tpe(t), ev(it) {}
    bool operator<(const TimePerEvent & r) { return tpe < r.tpe; }
};


void PerformanceStatistics::savePartialStatistics() {
    std::list<TimePerEvent> v;
    for (std::map<std::string, EventStats>::iterator it = handleTimeStatistics.begin(); it != handleTimeStatistics.end(); it++) {
        if (it->second.partialNumEvents > 0)
            v.push_back(TimePerEvent(it->second.partialHandleTime / it->second.partialNumEvents, it));
    }
    v.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    os << "Real Time: " << sim.getRealTime() << "   Sim Time: " << sim.getCurrentTime() << std::endl;
    for (std::list<TimePerEvent>::iterator it = v.begin(); it != v.end(); it++) {
        os << "   " << it->ev->first << ": "
            << it->ev->second.partialNumEvents << " events at "
            << it->tpe << " us/ev" << std::endl;
        // Reset partial counters
        it->ev->second.partialNumEvents = 0;
        it->ev->second.partialHandleTime = 0.0;
    }

    // Harvest memory occupation statistics
    os << "Database instances: " << SimAppDatabase::getTotalInstances() << " instances in " << SimAppDatabase::getTotalInstancesMem() << " bytes" << std::endl;
    os << "Database requests: " << SimAppDatabase::getTotalRequests() << " requests in " << SimAppDatabase::getTotalRequestsMem() << " bytes" << std::endl;
}


void PerformanceStatistics::saveTotalStatistics() {
    std::list<TimePerEvent> v;
    for (std::map<std::string, EventStats>::iterator it = handleTimeStatistics.begin(); it != handleTimeStatistics.end(); it++) {
        if (it->second.totalNumEvents > 0)
            v.push_back(TimePerEvent(it->second.totalHandleTime / it->second.totalNumEvents, it));
    }
    v.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    os << "Final Statistics" << std::endl;
    os << "Real Time: " << sim.getRealTime() << "   Sim Time: " << sim.getCurrentTime() << std::endl;
    for (std::list<TimePerEvent>::iterator it = v.begin(); it != v.end(); it++) {
        os << "   " << it->ev->first << ": "
            << it->ev->second.totalNumEvents << " events at "
            << it->tpe << " us/ev" << std::endl;
    }
}
