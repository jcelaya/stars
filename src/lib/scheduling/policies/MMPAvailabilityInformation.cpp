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
        if (unsigned int memRange = reference->maxM - reference->minM) {
            double loss = ((double)sum.accumM / memRange / sum.value);
            if (((minM - reference->minM) * numIntervals / memRange) != ((r.minM - reference->minM) * numIntervals / memRange))
                loss += 100.0;
            result += loss;
        }
        if (unsigned int diskRange = reference->maxD - reference->minD) {
            double loss = ((double)sum.accumD / diskRange / sum.value);
            if (((minD - reference->minD) * numIntervals / diskRange) != ((r.minD - reference->minD) * numIntervals / diskRange))
                loss += 100.0;
            result += loss;
        }
        if (unsigned int powerRange = reference->maxP - reference->minP) {
            double loss = ((double)sum.accumP / powerRange / sum.value);
            if (((minP - reference->minP) * numIntervals / powerRange) != ((r.minP - reference->minP) * numIntervals / powerRange))
                loss += 100.0;
            result += loss;
        }
        if (int64_t timeRange = (reference->maxT - reference->minT).microseconds()) {
            //double loss = ((double)sum.accumT.microseconds() / timeRange / sum.value);
            Time minT = maxT < r.maxT ? maxT : r.maxT;
            double timeScale = (minT - reference->minT).seconds() + 1.0;
            double totalLoss = sum.accumT.seconds() / timeScale;
            double loss = sum.accumT.seconds() / timeScale / sum.value;
            if (((maxT - reference->minT).microseconds() * numIntervals / timeRange) != ((r.maxT - reference->minT).microseconds() * numIntervals / timeRange))
                loss += 100.0;
            result += loss;
        }
//  if (uint64_t timeRange = reference->maxT > Time::getCurrentTime() ? (reference->maxT - Time::getCurrentTime()).microseconds() : 0) {
//   double loss = ((double)sum.accumT.microseconds() / timeRange / sum.value);
//   if (((maxT - reference->minT).microseconds() * numIntervals / timeRange) != ((r.maxT - reference->minT).microseconds() * numIntervals / timeRange))
//    loss += 100.0;
//   result += loss;
//  }
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
// if (uint64_t timeRange = reference->maxT > Time::getCurrentTime() ? (reference->maxT - Time::getCurrentTime()).microseconds() : 0) {
//  if (((maxT - reference->minT).microseconds() * numIntervals / timeRange) != ((r.maxT - reference->minT).microseconds() * numIntervals / timeRange))
//   return true;
// }
    return false;
}


void MMPAvailabilityInformation::MDPTCluster::aggregate(const MDPTCluster & r) {
    // Update minimums/maximums and sum up values
    uint32_t newMinM = minM, newMinD = minD, newMinP = minP;
    Time newMaxT = maxT;
    switch (aggrMethod) {
    case MEAN_FULL:
        newMinM = (minM * value + r.minM * r.value) / (value + r.value);
        newMinD = (minD * value + r.minD * r.value) / (value + r.value);
        newMinP = (minP * value + r.minP * r.value) / (value + r.value);
        if (maxT > r.maxT) {
            newMaxT = r.maxT + Duration(maxT - r.maxT) * (value / (value + r.value));
        } else {
            newMaxT = maxT + Duration(r.maxT - maxT) * (r.value / (value + r.value));
        }
        break;
    case MEAN_QUEUE:
        if (newMinM > r.minM) newMinM = r.minM;
        if (newMinD > r.minD) newMinD = r.minD;
        newMinP = (minP * value + r.minP * r.value) / (value + r.value);
        if (maxT > r.maxT) {
            newMaxT = r.maxT + Duration(maxT - r.maxT) * (value / (value + r.value));
        } else {
            newMaxT = maxT + Duration(r.maxT - maxT) * (r.value / (value + r.value));
        }
        break;
    default:
        if (newMinM > r.minM) newMinM = r.minM;
        if (newMinD > r.minD) newMinD = r.minD;
        if (newMinP > r.minP) newMinP = r.minP;
        if (newMaxT < r.maxT) newMaxT = r.maxT;
        break;
    }
    accumM += value * abs(minM - newMinM) + r.accumM + r.value * abs(r.minM - newMinM);
    accumD += value * abs(minD - newMinD) + r.accumD + r.value * abs(r.minD - newMinD);
    accumP += value * abs(minP - newMinP) + r.accumP + r.value * abs(r.minP - newMinP);
    accumT += (newMaxT > maxT ? newMaxT - maxT : maxT - newMaxT) * value
            + r.accumT + (newMaxT > r.maxT ? newMaxT - r.maxT : r.maxT - newMaxT) * r.value;
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
    minT = maxT = minQueue = maxQueue = end;
    summary.pushBack(MDPTCluster(this, mem, disk, power, end));
}


void MMPAvailabilityInformation::join(const MMPAvailabilityInformation & r) {
    if (!r.summary.isEmpty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating two summaries:";
        // Aggregate min queue time
        if (r.minQueue < minQueue)
            minQueue = r.minQueue;

        if (r.maxQueue > maxQueue)
            maxQueue = r.maxQueue;

        if (summary.isEmpty()) {
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
        summary.merge(r.summary);
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++) {
            if (summary[i].maxT < current) {
                // Adjust max and accum time
                summary[i].accumT = Duration(0.0);
                summary[i].maxT = current;
            }
            summary[i].setReference(this);
        }

        // Reorganize so that clusters are ordered by minT value
        //sort(&summary[0], &summary[summary.getSize()]);
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
    for (unsigned int i = 0; i < summary.getSize(); i++) {
        MDPTCluster & c = summary[i];
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
