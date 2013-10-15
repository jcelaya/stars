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


#include <fstream>

#include <limits>
#include <algorithm>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_array.hpp>
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


double FSPAvailabilityInformation::MDZCluster::distance(const MDZCluster & r, MDZCluster & sum) const {
    sum = *this;
    sum.aggregate(r);
    return sum.minM.norm(reference->memoryRange, sum.value)
            + sum.minD.norm(reference->diskRange, sum.value)
            + reference->slownessSquareDiff ? sum.accumZsq / (sum.value * reference->slownessSquareDiff) : 0.0;
}


bool FSPAvailabilityInformation::MDZCluster::far(const MDZCluster & r) const {
    return minM.far(r.minM, reference->memoryRange, numIntervals) ||
            minD.far(r.minD, reference->diskRange, numIntervals);/* ||
            (reference->slownessSquareDiff &&
                floor(maxZ.sqdiff(reference->minZ, reference->lengthHorizon) * numIntervals / reference->slownessSquareDiff) !=
                floor(r.maxZ.sqdiff(r.reference->minZ, reference->lengthHorizon) * numIntervals / reference->slownessSquareDiff));*/
}


void FSPAvailabilityInformation::MDZCluster::aggregate(const MDZCluster & r) {
    LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating " << *this << " and " << r;
    ZAFunction newMaxL;
    accumZsq += accumZsq + r.accumZsq
            + newMaxL.maxAndLoss(maxZ, r.maxZ, value, r.value, accumZmax, r.accumZmax, reference->lengthHorizon);
    accumZmax.maxDiff(maxZ, r.maxZ, value, r.value, accumZmax, r.accumZmax);
    maxZ = newMaxL;
    minM.aggregate(value, r.minM, r.value);
    minD.aggregate(value, r.minD, r.value);
    value = value + r.value;
}


void FSPAvailabilityInformation::MDZCluster::reduce() {
    accumZsq += value * maxZ.reduceMax(reference->lengthHorizon);
    accumZmax.reduceMax(reference->lengthHorizon);
}


void FSPAvailabilityInformation::setAvailability(uint32_t m, uint32_t d, const FSPTaskList & curTasks, double power) {
    memoryRange.setLimits(m);
    diskRange.setLimits(d);
    slownessRange.setLimits(curTasks.getSlowness());   // curTasks must be sorted!!
    summary.clear();
    summary.push_back(MDZCluster(m, d, curTasks, power));
    minZ = maxZ = summary.front().maxZ;
    lengthHorizon = minZ.getHorizon();
}


std::list<FSPAvailabilityInformation::MDZCluster *> FSPAvailabilityInformation::getFunctions(const TaskDescription & req) {
    std::list<MDZCluster *> f;
    for (auto & i : summary)
        if (i.fulfills(req))
            f.push_back(&i);
    return f;
}


void FSPAvailabilityInformation::removeClusters(const std::list<MDZCluster *> & clusters) {
    auto it = summary.begin();
    for (auto c : clusters) {
        while (it != summary.end() && &(*it) != c)
            ++it;
        if (it != summary.end())
            it = summary.erase(it);
    }
}


double FSPAvailabilityInformation::getSlowestMachine() const {
    return maxZ.getSlowestMachine();
}


void FSPAvailabilityInformation::join(const FSPAvailabilityInformation & r) {
    if (!r.summary.empty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating two summaries:";

        if (summary.empty()) {
            // operator= forbidden
            memoryRange = r.memoryRange;
            diskRange = r.diskRange;
            minZ = r.minZ;
            maxZ = r.maxZ;
            lengthHorizon = r.lengthHorizon;
            slownessRange = r.slownessRange;
        } else {
            memoryRange.extend(r.memoryRange);
            diskRange.extend(r.diskRange);
            minZ.min(minZ, r.minZ);
            maxZ.max(maxZ, r.maxZ);
            if (lengthHorizon < r.lengthHorizon)
                lengthHorizon = r.lengthHorizon;
            slownessRange.extend(r.slownessRange);
        }
        summary.insert(summary.end(), r.summary.begin(), r.summary.end());
        if (firstModified > r.firstModified)
            firstModified = r.firstModified;
        if (lastModified < r.lastModified)
            lastModified = r.lastModified;
    }
}


void saveToFile(FSPAvailabilityInformation * fspai) {
    std::ofstream oss("fsptest.dat");
    msgpack::packer<std::ostream> pk(&oss);
    fspai->pack(pk);
}


void FSPAvailabilityInformation::reduce() {
    // Set up clustering variables
    slownessSquareDiff = maxZ.sqdiff(minZ, lengthHorizon);
    for (auto & i : summary)
        i.reference = this;
    //FSPAvailabilityInformation * copy = this->clone();
    boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
    summary.cluster(numClusters);
    boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time();
    long mus = (end - start).total_microseconds();
    LogMsg("Ex.RI.Aggr.FSP", INFO) << "Clustering lasted " << mus << " us";
    start = end;
    for (auto & i : summary)
        i.reduce();
    end = boost::posix_time::microsec_clock::local_time();
    LogMsg("Ex.RI.Aggr.FSP", INFO) << "Reduction lasted " << (end - start).total_microseconds() << " us";
//    if (mus > 30000)
//        saveToFile(copy);
//    delete copy;
}


void FSPAvailabilityInformation::output(std::ostream & os) const {
    os << slownessRange.getMin() << "s/i";
    if (!summary.empty()) {
        os << LogMsg::indent << "  (" << minZ << ", " << maxZ << ") {" << LogMsg::indent;
        for (auto & i : summary)
            os << "    " << i << LogMsg::indent;
        os << "  }";
    }
}

}
