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
namespace fs = boost::filesystem;
using namespace std;


void PerformanceStatistics::openStatsFile() {
    os.open(Simulator::getInstance().getResultDir() / fs::path("performance.stat"));
}


void PerformanceStatistics::startEvent(const std::string & ev) {
    Simulator & sim = Simulator::getInstance();
    uint32_t node = sim.isInSimulation() ? sim.getCurrentNode().getLocalAddress().getIPNum() : 0;
    start[node][ev] = boost::posix_time::microsec_clock::local_time();
}


void PerformanceStatistics::endEvent(const std::string & ev) {
    Simulator & sim = Simulator::getInstance();
    uint32_t node = sim.isInSimulation() ? sim.getCurrentNode().getLocalAddress().getIPNum() : 0;
    boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time(), s = start[node][ev];
    boost::mutex::scoped_lock lock(m);
    EventStats & es = statsPerEvent[ev];
    es.partialNumEvents++;
    es.totalNumEvents++;
    es.partialHandleTime += (end - s).total_microseconds();
    es.totalHandleTime += (end - s).total_microseconds();
}


PerformanceStatistics::EventStats PerformanceStatistics::getEvent(const std::string & ev) const {
    map<string, EventStats>::const_iterator it = statsPerEvent.find(ev);
    if (it != statsPerEvent.end())
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
    // NOTE: Not protected by the mutex, this method should be called always from just one thread
    list<TimePerEvent> v;
    for (map<string, EventStats>::iterator it = statsPerEvent.begin(); it != statsPerEvent.end(); it++) {
        if (it->second.partialNumEvents > 0)
            v.push_back(TimePerEvent(it->second.partialHandleTime / it->second.partialNumEvents, it));
    }
    v.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    os << "Real Time: " << sim.getRealTime() << "   Sim Time: " << Time::getCurrentTime() << endl;
    for (list<TimePerEvent>::iterator it = v.begin(); it != v.end(); it++) {
        os << "   " << it->ev->first << ": "
        << it->ev->second.partialNumEvents << " events at "
        << it->tpe << " us/ev" << endl;
        // Reset partial counters
        it->ev->second.partialNumEvents = 0;
        it->ev->second.partialHandleTime = 0.0;
    }

    // TODO: Harvest memory occupation statistics
}


void PerformanceStatistics::saveTotalStatistics() {
    // NOTE: Not protected by the mutex, this method should be called always from just one thread
    list<TimePerEvent> v;
    for (map<string, EventStats>::iterator it = statsPerEvent.begin(); it != statsPerEvent.end(); it++) {
        if (it->second.totalNumEvents > 0)
            v.push_back(TimePerEvent(it->second.totalHandleTime / it->second.totalNumEvents, it));
    }
    v.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    os << "Final Statistics" << endl;
    os << "Real Time: " << sim.getRealTime() << "   Sim Time: " << Time::getCurrentTime() << endl;
    for (list<TimePerEvent>::iterator it = v.begin(); it != v.end(); it++) {
        os << "   " << it->ev->first << ": "
        << it->ev->second.totalNumEvents << " events at "
        << it->tpe << " us/ev" << endl;
    }
}
