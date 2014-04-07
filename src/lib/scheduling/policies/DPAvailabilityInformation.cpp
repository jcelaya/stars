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
#include <list>
#include <cmath>
#include <limits>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "Logger.hpp"
#include "DPAvailabilityInformation.hpp"
using namespace std;


REGISTER_MESSAGE(DPAvailabilityInformation);


unsigned int DPAvailabilityInformation::numClusters = 125;
unsigned int DPAvailabilityInformation::numIntervals = 5;


void DPAvailabilityInformation::MDFCluster::aggregate(const MDFCluster & l, const MDFCluster & r) {
    Logger::msg("Ex.RI.Aggr", DEBUG, "Aggregating ", *this, " and ", r);
    reference = l.reference;
    // Update minimums/maximums and sum up values
    uint32_t newMinM = l.minM < r.minM ? l.minM : r.minM;
    uint32_t newMinD = l.minD < r.minD ? l.minD : r.minD;
    double ldm = l.minM - newMinM, rdm = r.minM - newMinM;
    accumMsq = l.accumMsq + l.value * ldm * ldm + 2 * ldm * l.accumMln
               + r.accumMsq + r.value * rdm * rdm + 2 * rdm * r.accumMln;
    accumMln = l.accumMln + l.value * ldm + r.accumMln + r.value * rdm;
    double ldd = l.minD - newMinD, rdd = r.minD - newMinD;
    accumDsq = l.accumDsq + l.value * ldd * ldd + 2 * ldd * l.accumDln
               + r.accumDsq + r.value * rdd * rdd + 2 * rdd * r.accumDln;
    accumDln = l.accumDln + l.value * ldd + r.accumDln + r.value * rdd;

    stars::LDeltaFunction newMinA;
    accumAsq = l.accumAsq + r.accumAsq
               + newMinA.minAndLoss(l.minA, r.minA, l.value, r.value, l.accumMaxA, r.accumMaxA, reference->aggregationTime, reference->horizon);
    accumMaxA.max(l.accumMaxA, r.accumMaxA);

    minM = newMinM;
    minD = newMinD;
    minA = std::move(newMinA);
    value = l.value + r.value;
}


void DPAvailabilityInformation::MDFCluster::aggregate(const MDFCluster & r) {
    aggregate(*this, r);
}


double DPAvailabilityInformation::MDFCluster::distance(const MDFCluster & r, MDFCluster & sum) const {
    sum.aggregate(*this, r);
    double result = 0.0;
    if (reference) {
        if (reference->memRange) {
            double loss = ((double)sum.accumMsq / (sum.value * reference->memRange * reference->memRange));
            result += loss;
        }
        if (reference->diskRange) {
            double loss = ((double)sum.accumDsq / (sum.value * reference->diskRange * reference->diskRange));
            result += loss;
        }
        if (reference->availRange) {
            double loss = (sum.accumAsq / reference->availRange / sum.value);
            result += loss;
        }
    }
    return result;
}


bool DPAvailabilityInformation::MDFCluster::far(const MDFCluster & r) const {
    if (reference->memRange) {
        if (((minM - reference->minM) * numIntervals / reference->memRange) != ((r.minM - reference->minM) * numIntervals / reference->memRange))
            return true;
    }
    if (reference->diskRange) {
        if (((minD - reference->minD) * numIntervals / reference->diskRange) != ((r.minD - reference->minD) * numIntervals / reference->diskRange))
            return true;
    }
    if (minA.isFree() != r.minA.isFree()) return true;
// TODO
    if (reference->availRange) {
        if (floor(minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)
                != floor(r.minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)) {
            return true;
        }
    }
    return false;
}


void DPAvailabilityInformation::MDFCluster::reduce() {
    accumAsq += minA.reduceMin(value, accumMaxA, reference->aggregationTime, reference->horizon);
    accumMaxA.reduceMax(reference->aggregationTime, reference->horizon);
}


void DPAvailabilityInformation::addNode(uint32_t mem, uint32_t disk, double power, const std::list<std::shared_ptr<Task> > & queue) {
    MDFCluster tmp(mem, disk, power, queue);
    summary.push_back(tmp);
    if (summary.size() == 1) {
        minM = maxM = mem;
        minD = maxD = disk;
        minA = maxA = tmp.minA;
        horizon = tmp.minA.getHorizon();
    } else {
        if (minM > mem) minM = mem;
        if (maxM < mem) maxM = mem;
        if (minD > disk) minD = disk;
        if (maxD < disk) maxD = disk;
        minA.min(minA, tmp.minA);
        maxA.max(maxA, tmp.minA);
        if (horizon < tmp.minA.getHorizon()) horizon = tmp.minA.getHorizon();
    }
}


void DPAvailabilityInformation::join(const DPAvailabilityInformation & r) {
    if (!r.summary.empty()) {
        Logger::msg("Ex.RI.Aggr", DEBUG, "Aggregating two summaries:");

        if (summary.empty()) {
            // operator= forbidden
            minM = r.minM;
            maxM = r.maxM;
            minD = r.minD;
            maxD = r.maxD;
            minA = r.minA;
            maxA = r.maxA;
            horizon = r.horizon;
        } else {
            if (minM > r.minM) minM = r.minM;
            if (maxM < r.maxM) maxM = r.maxM;
            if (minD > r.minD) minD = r.minD;
            if (maxD < r.maxD) maxD = r.maxD;
            minA.min(minA, r.minA);
            maxA.max(maxA, r.maxA);
            if (horizon < r.horizon) horizon = r.horizon;
        }
        summary.insert(summary.end(), r.summary.begin(), r.summary.end());
    }
}


void DPAvailabilityInformation::reduce() {
    for (auto & i : summary)
        i.setReference(this);
    // Set up clustering variables
    aggregationTime = Time::getCurrentTime();
    memRange = maxM - minM;
    diskRange = maxD - minD;
    availRange = maxA.sqdiff(minA, aggregationTime, horizon);
    summary.cluster(numClusters);
    for (auto & i : summary)
        i.reduce();
}


void DPAvailabilityInformation::getAvailability(list<AssignmentInfo> & ai, const TaskDescription & desc) const {
    Logger::msg("Ex.RI.Comp", DEBUG, "Looking on ", *this);
    Time now = Time::getCurrentTime();
    if (desc.getDeadline() > now) {
        // Make a list of suitable cells
        for (auto & i : summary) {
            unsigned long int avail = i.minA.getAvailabilityBefore(desc.getDeadline());
            unsigned int availNow = i.minA.getAvailabilityBefore(now);
            avail = avail > availNow ? avail - availNow : 0;
            if (i.value > 0 && avail >= desc.getLength() && i.minM >= desc.getMaxMemory() && i.minD >= desc.getMaxDisk()) {
                unsigned int numTasks = avail / desc.getLength();
                unsigned long int restAvail = avail % desc.getLength();
                ai.push_back(AssignmentInfo(const_cast<MDFCluster *>(&i), i.value * numTasks, i.minM - desc.getMaxMemory(), i.minD - desc.getMaxDisk(), restAvail));
            }
        }
    }
}


void DPAvailabilityInformation::update(const list<DPAvailabilityInformation::AssignmentInfo> & ai, const TaskDescription & desc) {
    // For each cluster
    for (list<AssignmentInfo>::const_iterator it = ai.begin(); it != ai.end(); it++) {
        MDFCluster & cluster = *it->cluster;
        double avail = cluster.minA.getAvailabilityBefore(desc.getDeadline());
        uint64_t tasksPerNode = avail / desc.getLength();
        if (tasksPerNode > 0) {
            unsigned int numNodes = it->numTasks / tasksPerNode;
            if (it->numTasks % tasksPerNode) numNodes++;

            // Update the old one, just take out the affected nodes
            // NOTE: do not touch accum values, do not know how...
            cluster.value -= numNodes;
            // Create new one
            MDFCluster tmp(cluster);
            tmp.value = numNodes;
            if (tasksPerNode > it->numTasks)
                tmp.minA.update(desc.getLength() * it->numTasks, desc.getDeadline(), horizon);
            else
                tmp.minA.update(desc.getLength() * tasksPerNode, desc.getDeadline(), horizon);

            summary.push_back(tmp);
            minA.min(minA, tmp.minA);
        }
    }
}


void DPAvailabilityInformation::output(std::ostream& os) const {
    for (auto & i : summary)
        os << "(" << i << ')';
}
