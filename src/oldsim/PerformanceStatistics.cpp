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


void PerformanceStatistics::EventStats::end() {
    pt::time_duration span = pt::microsec_clock::local_time() - start;
    unsigned long int micros = span.total_microseconds();
    if (numEvents == 0 || minDuration > micros)
        minDuration = micros;
    if (maxDuration < micros)
        maxDuration = micros;
    numEvents++;
    handleTime += micros;
}


PerformanceStatistics::EventStats & PerformanceStatistics::EventStats::operator+=(const EventStats & r) {
    if (numEvents == 0 || minDuration > r.minDuration)
        minDuration = r.minDuration;
    if (maxDuration < r.maxDuration)
        maxDuration = r.maxDuration;
    numEvents += r.numEvents;
    handleTime += r.handleTime;
    return *this;
}


void PerformanceStatistics::openFile(const fs::path & statDir) {
    os.open(statDir / fs::path("perf.stat"));
    lastPartialSave = pt::seconds(0);
}


void PerformanceStatistics::startEvent(const std::string & ev) {
    partialStatsPerEvent[ev].start = pt::microsec_clock::local_time();
}


void PerformanceStatistics::endEvent(const std::string & ev) {
    partialStatsPerEvent[ev].end();
}


struct TimeSummary {
    double tpe;
    std::string eventName;
    PerformanceStatistics::EventStats & stats;
    TimeSummary(const std::string & name, PerformanceStatistics::EventStats & st)
        : tpe(st.handleTime / st.numEvents), eventName(name), stats(st) {}
    bool operator<(const TimeSummary & r) { return tpe < r.tpe; }

    friend std::ostream & operator<<(std::ostream & os, const TimeSummary & r) {
        return os << "   " << r.eventName << ": "
            << r.stats.numEvents << " events at "
            << r.tpe << " us/ev [" << r.stats.minDuration << ", " << r.stats.maxDuration << "]"
            << std::endl;
    }
};


void PerformanceStatistics::copyPartialToTotal() {
    for (auto & partialStat : partialStatsPerEvent) {
        statsPerEvent[partialStat.first] += partialStat.second;
    }
}


void PerformanceStatistics::savePartialStatistics() {
    copyPartialToTotal();

    std::list<TimeSummary> summaries;
    for (auto & it : partialStatsPerEvent) {
        if (it.second.numEvents > 0) {
            summaries.push_back(TimeSummary(it.first, it.second));
        }
    }
    summaries.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    pt::time_duration realTime = sim.getRealTime();
    os << "Real Time: " << realTime << "   Sim Time: " << sim.getCurrentTime() << std::endl;
    pt::time_duration elapsed = realTime - lastPartialSave;
    lastPartialSave = realTime;
    double eventTime = 0.0;
    for (auto & summary : summaries) {
        os << summary;
        eventTime += summary.stats.handleTime;
        summary.stats.reset();
    }
    os << "   Other: " << (elapsed.total_microseconds() - eventTime) << " us" << std::endl;

    // Harvest memory occupation statistics
    os << "Database instances: " << SimAppDatabase::getTotalInstances() << " instances in " << SimAppDatabase::getTotalInstancesMem() << " bytes" << std::endl;
    os << "Database requests: " << SimAppDatabase::getTotalRequests() << " requests in " << SimAppDatabase::getTotalRequestsMem() << " bytes" << std::endl;
}


void PerformanceStatistics::saveTotalStatistics() {
    copyPartialToTotal();

    std::list<TimeSummary> summaries;
    for (auto & it : statsPerEvent) {
        if (it.second.numEvents > 0) {
            summaries.push_back(TimeSummary(it.first, it.second));
        }
    }
    summaries.sort();

    // Save statistics
    Simulator & sim = Simulator::getInstance();
    os << "Final Statistics" << std::endl;
    os << "Real Time: " << sim.getRealTime() << "   Sim Time: " << sim.getCurrentTime() << std::endl;
    for (auto & summary : summaries) {
        os << summary;
    }
}
