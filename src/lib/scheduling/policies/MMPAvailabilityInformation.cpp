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
using namespace boost;


REGISTER_MESSAGE(MMPAvailabilityInformation);


unsigned int MMPAvailabilityInformation::numClusters = 256;
unsigned int MMPAvailabilityInformation::numIntervals = 4;
int MMPAvailabilityInformation::aggrMethod = MMPAvailabilityInformation::MINIMUM;


double MMPAvailabilityInformation::MDPTCluster::distance(const MDPTCluster & r, MDPTCluster & sum) const {
    sum = *this;
    sum.aggregate(r);
    double result = 0.0;
    if (reference) {
        if (double memRange = reference->maxM - reference->minM) {
            double loss = sum.accumMsq / (sum.value * memRange * memRange);
            result += loss;
        }
        if (double diskRange = reference->maxD - reference->minD) {
            double loss = sum.accumDsq / (sum.value * diskRange * diskRange);
            result += loss;
        }
        if (double powerRange = reference->maxP - reference->minP) {
            double loss = sum.accumPsq / (sum.value * powerRange * powerRange);
            result += loss;
        }
        if (double timeRange = (reference->maxT - reference->minT).microseconds()) {
            double loss = sum.accumTsq / (sum.value * timeRange * timeRange);
            result += loss;
        }
    }
    return result;
}


bool MMPAvailabilityInformation::MDPTCluster::far(const MDPTCluster & r) const {
    if (unsigned int memRange = reference->maxM - reference->minM) {
        if (((minM - reference->minM) * numIntervals / memRange) != ((r.minM - reference->minM) * numIntervals / memRange))
            return true;
    }
    if (unsigned int diskRange = reference->maxD - reference->minD) {
        if (((minD - reference->minD) * numIntervals / diskRange) != ((r.minD - reference->minD) * numIntervals / diskRange))
            return true;
    }
    if (unsigned int powerRange = reference->maxP - reference->minP) {
        if (((minP - reference->minP) * numIntervals / powerRange) != ((r.minP - reference->minP) * numIntervals / powerRange))
            return true;
    }
    if (uint64_t timeRange = (reference->maxT - reference->minT).microseconds()) {
        if (((maxT - reference->minT).microseconds() * numIntervals / timeRange) != ((r.maxT - reference->minT).microseconds() * numIntervals / timeRange))
            return true;
    }
    return false;
}


void MMPAvailabilityInformation::MDPTCluster::aggregate(const MDPTCluster & r) {
    // Update minimums/maximums and sum up values
    uint32_t newMinM = minM, newMinD = minD, newMinP = minP;
    Time newMaxT = maxT;
    if (newMinM > r.minM) newMinM = r.minM;
    if (newMinD > r.minD) newMinD = r.minD;
    if (newMinP > r.minP) newMinP = r.minP;
    if (newMaxT < r.maxT) newMaxT = r.maxT;
    int64_t dm = minM - newMinM, rdm = r.minM - newMinM;
    accumMsq += value * dm * dm + 2 * dm * accumMln
                + r.accumMsq + r.value * rdm * rdm + 2 * rdm * r.accumMln;
    accumMln += value * dm + r.accumMln + r.value * rdm;
    int64_t dd = minD - newMinD, rdd = r.minD - newMinD;
    accumDsq += value * dd * dd + 2 * dd * accumDln
                + r.accumDsq + r.value * rdd * rdd + 2 * rdd * r.accumDln;
    accumDln += value * dd + r.accumDln + r.value * rdd;
    int64_t dp = minD - newMinD, rdp = r.minD - newMinD;
    accumPsq += value * dp * dp + 2 * dp * accumPln
                + r.accumPsq + r.value * rdp * rdp + 2 * rdp * r.accumPln;
    accumPln += value * dp + r.accumPln + r.value * rdp;
    int64_t dt = (maxT - newMaxT).microseconds(), rdt = (r.maxT - newMaxT).microseconds();
    accumTsq += value * dt * dt + 2 * dt * accumTln
                + r.accumTsq + r.value * rdt * rdt + 2 * rdt * r.accumTln;
    accumTln += value * dt + r.accumTln + r.value * rdt;
    minM = newMinM;
    minD = newMinD;
    minP = newMinP;
    maxT = newMaxT;
    value += r.value;
}


void MMPAvailabilityInformation::setQueueEnd(uint32_t mem, uint32_t disk, uint32_t power, Time end) {
    summary.clear();
    minM = maxM = mem;
    minD = maxD = disk;
    minP = maxP = power;
    minT = maxT = maxQueue = end;
    summary.push_back(MDPTCluster(this, mem, disk, power, end));
}


void MMPAvailabilityInformation::join(const MMPAvailabilityInformation & r) {
    if (!r.summary.empty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating two summaries:";
        // Aggregate max queue time
        if (r.maxQueue > maxQueue)
            maxQueue = r.maxQueue;

        if (summary.empty()) {
            minM = r.minM;
            maxM = r.maxM;
            minD = r.minD;
            maxD = r.maxD;
            minP = r.minP;
            maxP = r.maxP;
            minT = r.minT;
            maxT = r.maxT;
        } else {
            if (minM > r.minM) minM = r.minM;
            if (maxM < r.maxM) maxM = r.maxM;
            if (minD > r.minD) minD = r.minD;
            if (maxD < r.maxD) maxD = r.maxD;
            if (minP > r.minP) minP = r.minP;
            if (maxP < r.maxP) maxP = r.maxP;
            if (minT > r.minT) minT = r.minT;
            if (maxT < r.maxT) maxT = r.maxT;
        }

        // Create a vector of clusters with those clusters which min or max time is earlier than current time
        Time current = Time::getCurrentTime();

        std::list<MDPTCluster> tmpList(r.summary.begin(), r.summary.end());
        summary.merge(tmpList);
        for (auto & i : summary) {
            if (i.maxT < current) {
                // Adjust max and accum time
                i.accumTsq = i.accumTln = 0;
                i.maxT = current;
            }
            i.setReference(this);
        }

        // Do not clusterize at this moment, wait until serialization
        // The same with the cover
        if (minT < current) {
            minT = current;
            if (maxT < current)
                maxT = current;
        }
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
        if (c.maxT > start) start = c.maxT;
        if (start < req.getDeadline() && c.fulfills(req)) {
            double time = (req.getDeadline() - start).seconds();
            unsigned long int length = req.getLength() ? req.getLength() : 1000;   // A minimum length
            unsigned long int t = (time * c.minP) / length;
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
        (*it)->maxT = req.getDeadline();
    if (!clusters.empty() && maxT < req.getDeadline())
        maxT = req.getDeadline();
}
