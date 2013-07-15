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


#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/shared_array.hpp>
#include <limits>
#include <algorithm>
#include <sstream>
#include "Logger.hpp"
#include "FSPAvailabilityInformation.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
using std::vector;
using std::list;
using std::make_pair;


namespace stars {

REGISTER_MESSAGE(FSPAvailabilityInformation);


unsigned int FSPAvailabilityInformation::numClusters = 125;
unsigned int FSPAvailabilityInformation::numIntervals = 5;


double FSPAvailabilityInformation::MDLCluster::distance(const MDLCluster & r, MDLCluster & sum) const {
    sum = *this;
    sum.aggregate(r);
    return sum.minM.norm(reference->memoryRange, sum.value)
            + sum.minD.norm(reference->diskRange, sum.value)
            + reference->slownessSquareDiff ? sum.accumLsq / (sum.value * reference->slownessSquareDiff) : 0.0;
}


bool FSPAvailabilityInformation::MDLCluster::far(const MDLCluster & r) const {
    return minM.far(r.minM, reference->memoryRange, numIntervals) ||
            minD.far(r.minD, reference->diskRange, numIntervals) ||
            (reference->slownessSquareDiff &&
                floor(maxL.sqdiff(reference->minL, reference->lengthHorizon) * numIntervals / reference->slownessSquareDiff) !=
                floor(r.maxL.sqdiff(r.reference->minL, reference->lengthHorizon) * numIntervals / reference->slownessSquareDiff));
}


void FSPAvailabilityInformation::MDLCluster::aggregate(const MDLCluster & r) {
    LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating " << *this << " and " << r;
    ZAFunction newMaxL;
    accumLsq += accumLsq + r.accumLsq
            + newMaxL.maxAndLoss(maxL, r.maxL, value, r.value, accumMaxL, r.accumMaxL, reference->lengthHorizon);
    accumMaxL.maxDiff(maxL, r.maxL, value, r.value, accumMaxL, r.accumMaxL);
    maxL = newMaxL;
    minM.aggregate(value, r.minM, r.value);
    minD.aggregate(value, r.minD, r.value);
    value = value + r.value;
}


void FSPAvailabilityInformation::MDLCluster::reduce() {
    accumLsq += value * maxL.reduceMax(reference->lengthHorizon);
    accumMaxL.reduceMax(reference->lengthHorizon);
}


void FSPAvailabilityInformation::setAvailability(uint32_t m, uint32_t d, const FSPTaskList & curTasks, double power) {
    memoryRange.setLimits(m);
    diskRange.setLimits(d);
    slownessRange.setLimits(curTasks.getSlowness());   // curTasks must be sorted!!
    summary.clear();
    summary.push_back(MDLCluster(m, d, curTasks, power));
    minL = maxL = summary.front().maxL;
    lengthHorizon = minL.getHorizon();
}


std::list<FSPAvailabilityInformation::MDLCluster *> FSPAvailabilityInformation::getFunctions(const TaskDescription & req) {
    std::list<MDLCluster *> f;
    for (auto & i : summary)
        if (i.fulfills(req))
            f.push_back(&i);
    return f;
}


double FSPAvailabilityInformation::getSlowestMachine() const {
    return maxL.getSlowestMachine();
}


void FSPAvailabilityInformation::join(const FSPAvailabilityInformation & r) {
    if (r.summary.empty()) {
        reset();   // Invalidate!!
    } else if (!summary.empty()) {
        memoryRange.extend(r.memoryRange);
        diskRange.extend(r.diskRange);
        minL.min(minL, r.minL);
        maxL.max(maxL, r.maxL);
        if (lengthHorizon < r.lengthHorizon)
            lengthHorizon = r.lengthHorizon;
        slownessRange.extend(r.slownessRange);
        summary.insert(summary.end(), r.summary.begin(), r.summary.end());
    }
}


void FSPAvailabilityInformation::reduce() {
    // Set up clustering variables
    slownessSquareDiff = maxL.sqdiff(minL, lengthHorizon);
    for (auto & i : summary)
        i.reference = this;
    summary.cluster(numClusters);
    for (auto & i : summary)
        i.reduce();
}


void FSPAvailabilityInformation::output(std::ostream & os) const {
    os << slownessRange.getMin() << "s/i, ";
    os << '(' << minL << ", " << maxL << ") (";
    for (auto & i : summary)
        os << i << ',';
    os << ')';
}

}
