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
#include <sstream>
#include "AggregationTest.hpp"
#include "FSPAvailabilityInformation.hpp"
#include "FSPScheduler.hpp"
#include "util/MemoryManager.hpp"
using namespace stars;

template<> struct Priv<FSPAvailabilityInformation> {
    LAFunction totalAvail;
    LAFunction maxAvail;
};


template<> AggregationTestImpl<FSPAvailabilityInformation>::AggregationTestImpl() : AggregationTest("fsp_mem_disk_slowness.stat", 2) {
    stars::ClusteringList<FSPAvailabilityInformation::MDLCluster>::setDistVectorSize(20);
    LAFunction::setNumPieces(8);
}


template<> boost::shared_ptr<FSPAvailabilityInformation> AggregationTestImpl<FSPAvailabilityInformation>::createInfo(const AggregationTestImpl::Node & n) {
    static LAFunction dummy;
    boost::shared_ptr<FSPAvailabilityInformation> s(new FSPAvailabilityInformation);
    FSPTaskList proxys(gen.createRandomQueue(n.power));
    s->setAvailability(n.mem, n.disk, proxys, n.power);
    const LAFunction & maxL = s->getSummary().front().getMaximumSlowness();
    if (privateData.maxAvail == LAFunction()) {
        privateData.maxAvail = maxL;
    } else {
        privateData.maxAvail.max(privateData.maxAvail, maxL);
        //privateData.minAvail.reduce();
    }
    privateData.totalAvail.maxDiff(dummy, dummy, 1, 1, maxL, privateData.totalAvail);
    return s;
}


template<> void AggregationTestImpl<FSPAvailabilityInformation>::computeResults(const boost::shared_ptr<FSPAvailabilityInformation> & summary) {
    static LAFunction dummy;
    unsigned long int minMem = nodes.size() * min_mem;
    unsigned long int minDisk = nodes.size() * min_disk;
    LAFunction maxAvail, aggrAvail;
    maxAvail.maxDiff(privateData.maxAvail, dummy, getNumNodes(), getNumNodes(), dummy, dummy);

    unsigned long int aggrMem = 0, aggrDisk = 0;
    double meanAccuracy = 0.0;
    const stars::ClusteringList<FSPAvailabilityInformation::MDLCluster> & clusters = summary->getSummary();
    for (auto & u : clusters) {
        aggrMem += u.getTotalMemory();
        aggrDisk += u.getTotalDisk();
        aggrAvail.maxDiff(u.getMaximumSlowness(), dummy, u.getValue(), u.getValue(), aggrAvail, dummy);
    }
    // TODO: The accuracy is not linear...
    double prevAccuracy = 100.0;
    uint64_t prevA = LAFunction::minTaskLength;
    double ah = privateData.totalAvail.getHorizon() * 1.2;
    uint64_t astep = (ah - LAFunction::minTaskLength) / 100;
    for (uint64_t a = LAFunction::minTaskLength; a < ah; a += astep) {
        double maxAvailBeforeIt = maxAvail.getSlowness(a);
        double totalAvailBeforeIt = maxAvailBeforeIt - privateData.totalAvail.getSlowness(a);
        double aggrAvailBeforeIt = maxAvailBeforeIt - aggrAvail.getSlowness(a);
        double accuracy = totalAvailBeforeIt > 0 ? (aggrAvailBeforeIt * 100.0) / totalAvailBeforeIt : 0.0;
        if (totalAvailBeforeIt + 1 < aggrAvailBeforeIt)
            LogMsg("test", ERROR) << "total availability is lower than aggregated... (" << totalAvailBeforeIt << " < " << aggrAvailBeforeIt << ')';
        meanAccuracy += (prevAccuracy + accuracy) * (a - prevA);
        prevAccuracy = accuracy;
        prevA = a;
    }
    meanAccuracy /= 2.0 * (ah - LAFunction::minTaskLength);

    results["M"].value(totalMem).value(minMem).value(aggrMem).value((aggrMem - minMem) * 100.0 / (totalMem - minMem));
    results["D"].value(totalDisk).value(minDisk).value(aggrDisk).value((aggrDisk - minDisk) * 100.0 / (totalDisk - minDisk));
    results["Z"].value(0.0).value(0.0).value(0.0).value(meanAccuracy);
}


AggregationTest & AggregationTest::getInstance() {
    static AggregationTestImpl<FSPAvailabilityInformation> instance;
    return instance;
}
