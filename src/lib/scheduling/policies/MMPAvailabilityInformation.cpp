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

#include <algorithm>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "Logger.hpp"
#include "MMPAvailabilityInformation.hpp"
#include "Time.hpp"
using namespace std;


REGISTER_MESSAGE(MMPAvailabilityInformation);


unsigned int MMPAvailabilityInformation::numClusters = 256;
unsigned int MMPAvailabilityInformation::numIntervals = 4;


void MMPAvailabilityInformation::setQueueEnd(int32_t mem, int32_t disk, int32_t power, Time end) {
    summary.clear();
    memoryRange.setLimits(mem);
    diskRange.setLimits(disk);
    powerRange.setLimits(power);
    queueRange.setLimits(end);
    summary.push_back(MDPTCluster(mem, disk, power, end));
}


void MMPAvailabilityInformation::join(const MMPAvailabilityInformation & r) {
    if (!r.summary.empty()) {
        Logger::msg("Ex.RI.Aggr", DEBUG, "Aggregating two summaries:");
        // Aggregate max queue time
        if (r.maxQueue > maxQueue)
            maxQueue = r.maxQueue;

        if (summary.empty()) {
            // operator= forbidden
            memoryRange = r.memoryRange;
            diskRange = r.diskRange;
            powerRange = r.powerRange;
            queueRange = r.queueRange;
        } else {
            memoryRange.extend(r.memoryRange);
            diskRange.extend(r.diskRange);
            powerRange.extend(r.powerRange);
            queueRange.extend(r.queueRange);
        }
        std::list<MDPTCluster> tmpList(r.summary.begin(), r.summary.end());
        summary.merge(tmpList);

        Time current = Time::getCurrentTime();
        for (auto & i : summary) {
            if (i.maxT.getValue() < current) {
                // Adjust max and accum time
                i.maxT = stars::MaxParameter<Time, int64_t>(current);
            }
        }
        // The same with the cover
        if (queueRange.getMin() < current)
            queueRange.setMinimum(current);
    }
}


Time MMPAvailabilityInformation::getAvailability(list<MDPTCluster *> & clusters,
        unsigned int numTasks, const TaskDescription & req) {
    TaskDescription tmp(req);
    Time max = Time::getCurrentTime(), min;
    int64_t d = 300000000;
    unsigned int t = 0;
    while (t < numTasks && d < 1000000000000000000L) {
        clusters.clear();
        min = max;
        max += Duration(d);
        d *= 2;
        tmp.setDeadline(max);
        t = getAvailability(clusters, tmp);
    }
    unsigned int last = 0;
    while (last != t) {
        clusters.clear();
        last = t;
        d /= 2;
        Time med = min + Duration(d);
        tmp.setDeadline(med);
        t = getAvailability(clusters, tmp);
        if (t < numTasks) min = med;
        else max = med;
    }

    return max;
}


unsigned int MMPAvailabilityInformation::getAvailability(list<MDPTCluster *> & clusters, const TaskDescription & req) {
    unsigned int result = 0;
    Time now = Time::getCurrentTime();
    for (auto & c : summary) {
        Time start = now;
        if (c.maxT.getValue() > start) start = c.maxT.getValue();
        if (start < req.getDeadline() && c.fulfills(req)) {
            double time = (req.getDeadline() - start).seconds();
            unsigned long int length = req.getLength() ? req.getLength() : 1000;   // A minimum length
            unsigned long int t = (time * c.minP.getValue()) / length;
            if (t != 0) {
                clusters.push_back(&c);
                result += t;
            }
        }
    }
    return result;
}


void MMPAvailabilityInformation::updateAvailability(const TaskDescription & req) {
    list<MDPTCluster *> clusters;
    getAvailability(clusters, req);
    for (list<MDPTCluster *>::iterator it = clusters.begin(); it != clusters.end(); it++)
        (*it)->maxT = stars::MaxParameter<Time, int64_t>(req.getDeadline());
    if (!clusters.empty() && queueRange.getMax() < req.getDeadline())
        queueRange.setMaximum(req.getDeadline());
}
