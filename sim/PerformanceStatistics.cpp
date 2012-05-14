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

#include <list>
#include "PerformanceStatistics.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost::posix_time;


void PerformanceStatistics::openFile(const fs::path & statDir) {
    os.open(statDir / fs::path("perf.stat"));
}


void PerformanceStatistics::startEvent(const std::string & ev) {
    handleTimeStatistics[ev].start = microsec_clock::local_time();
}


void PerformanceStatistics::endEvent(const std::string & ev) {
    ptime end = microsec_clock::local_time();
    EventStats & es = handleTimeStatistics[ev];
    es.partialNumEvents++;
    es.totalNumEvents++;
    es.partialHandleTime += (end - es.start).total_microseconds();
    es.totalHandleTime += (end - es.start).total_microseconds();
}


PerformanceStatistics::EventStats PerformanceStatistics::getEvent(const std::string & ev) const {
    map<string, EventStats>::const_iterator it = handleTimeStatistics.find(ev);
    if (it != handleTimeStatistics.end())
        return it->second;
    else return EventStats();
}


struct TimePerEvent {
    double tpe;
    map<string, PerformanceStatistics::EventStats>::iterator ev;
    TimePerEvent(double t, map<string, PerformanceStatistics::EventStats>::iterator it) : tpe(t), ev(it) {}
    bool operator<(const TimePerEvent & r) {
        return tpe < r.tpe;
    }
};


void PerformanceStatistics::savePartialStatistics() {
    list<TimePerEvent> v;
    for (map<string, EventStats>::iterator it = handleTimeStatistics.begin(); it != handleTimeStatistics.end(); it++) {
        if (it->second.partialNumEvents > 0)
            v.push_back(TimePerEvent(it->second.partialHandleTime / it->second.partialNumEvents, it));
    }
    v.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    os << "Real Time: " << sim.getRealTime() << "   Sim Time: " << sim.getCurrentTime() << endl;
    for (list<TimePerEvent>::iterator it = v.begin(); it != v.end(); it++) {
        os << "   " << it->ev->first << ": "
        << it->ev->second.partialNumEvents << " events at "
        << it->tpe << " us/ev" << endl;
        // Reset partial counters
        it->ev->second.partialNumEvents = 0;
        it->ev->second.partialHandleTime = 0.0;
    }

    // Harvest memory occupation statistics
    os << "Database applications: " << SimAppDatabase::getTotalApps() << " apps in " << SimAppDatabase::getTotalAppsMem() << " bytes" << endl;
    os << "Database instances: " << SimAppDatabase::getTotalInstances() << " instances in " << SimAppDatabase::getTotalInstancesMem() << " bytes" << endl;
    os << "Database requests: " << SimAppDatabase::getTotalRequests() << " requests in " << SimAppDatabase::getTotalRequestsMem() << " bytes" << endl;
}


void PerformanceStatistics::saveTotalStatistics() {
    list<TimePerEvent> v;
    for (map<string, EventStats>::iterator it = handleTimeStatistics.begin(); it != handleTimeStatistics.end(); it++) {
        if (it->second.totalNumEvents > 0)
            v.push_back(TimePerEvent(it->second.totalHandleTime / it->second.totalNumEvents, it));
    }
    v.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    os << "Final Statistics" << endl;
    os << "Real Time: " << sim.getRealTime() << "   Sim Time: " << sim.getCurrentTime() << endl;
    for (list<TimePerEvent>::iterator it = v.begin(); it != v.end(); it++) {
        os << "   " << it->ev->first << ": "
        << it->ev->second.totalNumEvents << " events at "
        << it->tpe << " us/ev" << endl;
    }
}
