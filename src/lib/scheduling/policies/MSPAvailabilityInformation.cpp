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
#include "MSPAvailabilityInformation.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
using std::vector;
using std::list;
using std::make_pair;


namespace stars {

REGISTER_MESSAGE(MSPAvailabilityInformation);


unsigned int MSPAvailabilityInformation::numClusters = 125;
unsigned int MSPAvailabilityInformation::numIntervals = 5;


double MSPAvailabilityInformation::MDLCluster::distance(const MDLCluster & r, MDLCluster & sum) const {
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
        if (reference->slownessRange) {
            double loss = (sum.accumLsq / (sum.value * reference->slownessRange));
            result += loss;
        }
    }
    return result;
}


bool MSPAvailabilityInformation::MDLCluster::far(const MDLCluster & r) const {
    if (reference->memRange) {
        if (((minM - reference->minM) * numIntervals / reference->memRange) != ((r.minM - reference->minM) * numIntervals / reference->memRange))
            return true;
    }
    if (reference->diskRange) {
        if (((minD - reference->minD) * numIntervals / reference->diskRange) != ((r.minD - reference->minD) * numIntervals / reference->diskRange))
            return true;
    }
    if (reference->slownessRange) {
        if (floor(maxL.sqdiff(reference->minL, reference->lengthHorizon) * numIntervals / reference->slownessRange) !=
                floor(r.maxL.sqdiff(r.reference->minL, reference->lengthHorizon) * numIntervals / reference->slownessRange))
            return true;
    }
    return false;
}


void MSPAvailabilityInformation::MDLCluster::aggregate(const MDLCluster & r) {
    aggregate(*this, r);
}


void MSPAvailabilityInformation::MDLCluster::aggregate(const MDLCluster & l, const MDLCluster & r) {
    LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating " << *this << " and " << r;
    reference = l.reference;
    // Update minimums/maximums and sum up values
    uint32_t newMinM = l.minM < r.minM ? l.minM : r.minM;
    uint32_t newMinD = l.minD < r.minD ? l.minD : r.minD;
    uint64_t ldm = l.minM - newMinM, rdm = r.minM - newMinM;
    accumMsq = l.accumMsq + l.value * ldm * ldm + 2 * ldm * l.accumMln
            + r.accumMsq + r.value * rdm * rdm + 2 * rdm * r.accumMln;
    accumMln = l.accumMln + l.value * ldm + r.accumMln + r.value * rdm;
    uint64_t ldd = l.minD - newMinD, rdd = r.minD - newMinD;
    accumDsq = l.accumDsq + l.value * ldd * ldd + 2 * ldd * l.accumDln
            + r.accumDsq + r.value * rdd * rdd + 2 * rdd * r.accumDln;
    accumDln = l.accumDln + l.value * ldd + r.accumDln + r.value * rdd;

    LAFunction newMaxL;
    accumLsq = l.accumLsq + r.accumLsq
            + newMaxL.maxAndLoss(l.maxL, r.maxL, l.value, r.value, l.accumMaxL, r.accumMaxL, reference->lengthHorizon);
    accumMaxL.maxDiff(l.maxL, r.maxL, l.value, r.value, l.accumMaxL, r.accumMaxL);

    minM = newMinM;
    minD = newMinD;
    maxL = newMaxL;
    value = l.value + r.value;
}


void MSPAvailabilityInformation::MDLCluster::reduce() {
//    accumLsq += maxL.reduceMax(value, reference->lengthHorizon);
//    accumMaxL.reduceMax(1, reference->lengthHorizon);
}


void MSPAvailabilityInformation::setAvailability(uint32_t m, uint32_t d, const TaskProxy::List & curTasks,
        const std::vector<double> & switchValues, double power, double minSlowness) {
    minM = maxM = m;
    minD = maxD = d;
    minimumSlowness = maximumSlowness = minSlowness;
    summary.clear();
    summary.push_back(MDLCluster(this, m, d, curTasks, switchValues, power));
    minL = maxL = summary.front().maxL;
    lengthHorizon = minL.getHorizon();
}


void MSPAvailabilityInformation::getFunctions(const TaskDescription & req, std::vector<std::pair<LAFunction *, unsigned int> > & f) {
    for (auto & i : summary)
        if (i.fulfills(req))
            f.push_back(std::make_pair(&i.maxL, i.value));
}


double MSPAvailabilityInformation::getSlowestMachine() const {
    return maxL.getSlowestMachine();
}


void MSPAvailabilityInformation::join(const MSPAvailabilityInformation & r) {
    if (!r.summary.empty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating two summaries:";

        if (summary.empty()) {
            // operator= forbidden
            minM = r.minM;
            maxM = r.maxM;
            minD = r.minD;
            maxD = r.maxD;
            minL = r.minL;
            maxL = r.maxL;
            lengthHorizon = r.lengthHorizon;
            minimumSlowness = r.minimumSlowness;
            maximumSlowness = r.maximumSlowness;
        } else {
            if (minM > r.minM) minM = r.minM;
            if (maxM < r.maxM) maxM = r.maxM;
            if (minD > r.minD) minD = r.minD;
            if (maxD < r.maxD) maxD = r.maxD;
            minL.min(minL, r.minL);
            maxL.max(maxL, r.maxL);
            if (lengthHorizon < r.lengthHorizon)
                lengthHorizon = r.lengthHorizon;
            if (minimumSlowness > r.minimumSlowness)
                minimumSlowness = r.minimumSlowness;
            if (maximumSlowness < r.maximumSlowness)
                maximumSlowness = r.maximumSlowness;
        }
        summary.insert(summary.end(), r.summary.begin(), r.summary.end());
    }
}


void MSPAvailabilityInformation::reduce() {
    // Set up clustering variables
    memRange = maxM - minM;
    diskRange = maxD - minD;
    slownessRange = maxL.sqdiff(minL, lengthHorizon);
    for (auto & i : summary)
        i.reference = this;
    summary.cluster(numClusters);
    for (auto & i : summary)
        i.reduce();
}


void MSPAvailabilityInformation::output(std::ostream & os) const {
    os << minimumSlowness << "s/i, ";
    os << '(' << minM << "MB, " << maxM << "MB) ";
    os << '(' << minD << "MB, " << maxD << "MB) ";
    os << '(' << minL << ", " << maxL << ") (";
    for (auto & i : summary)
        os << i << ',';
    os << ')';
}

}
