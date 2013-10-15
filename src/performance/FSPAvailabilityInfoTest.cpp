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
    ZAFunction totalAvail;
    ZAFunction maxAvail;
};


template<> AggregationTestImpl<FSPAvailabilityInformation>::AggregationTestImpl() : AggregationTest("fsp_mem_disk_slowness.stat", 2) {
    stars::ClusteringList<FSPAvailabilityInformation::MDZCluster>::setDistVectorSize(10);
    ZAFunction::setNumPieces(10);
    ZAFunction::setReductionQuality(1);
}


template<> boost::shared_ptr<FSPAvailabilityInformation> AggregationTestImpl<FSPAvailabilityInformation>::createInfo(const AggregationTestImpl::Node & n) {
    static ZAFunction dummy;
    boost::shared_ptr<FSPAvailabilityInformation> s(new FSPAvailabilityInformation);
    FSPTaskList proxys(gen.createRandomQueue(n.power));
    s->setAvailability(n.mem, n.disk, proxys, n.power);
    const ZAFunction & maxL = s->getSummary().front().getMaximumSlowness();
    if (privateData.maxAvail == ZAFunction()) {
        privateData.maxAvail = maxL;
    } else {
        privateData.maxAvail.max(privateData.maxAvail, maxL);
        //privateData.minAvail.reduce();
    }
    privateData.totalAvail.maxDiff(dummy, dummy, 1, 1, maxL, privateData.totalAvail);
    return s;
}


template<> void AggregationTestImpl<FSPAvailabilityInformation>::computeResults(const boost::shared_ptr<FSPAvailabilityInformation> & summary) {
    static ZAFunction dummy;
    unsigned long int minMem = nodes.size() * min_mem;
    unsigned long int minDisk = nodes.size() * min_disk;
    ZAFunction maxAvail, aggrAvail;
    maxAvail.maxDiff(privateData.maxAvail, dummy, getNumNodes(), getNumNodes(), dummy, dummy);

    unsigned long int aggrMem = 0, aggrDisk = 0;
    double meanAccuracy = 0.0;
    const stars::ClusteringList<FSPAvailabilityInformation::MDZCluster> & clusters = summary->getSummary();
    for (auto & u : clusters) {
        aggrMem += u.getTotalMemory();
        aggrDisk += u.getTotalDisk();
        aggrAvail.maxDiff(u.getMaximumSlowness(), dummy, u.getValue(), u.getValue(), aggrAvail, dummy);
    }
    // TODO: The accuracy is not linear...
    double prevAccuracy = 100.0;
    double prevA = ZAFunction::minTaskLength;
    double ah = privateData.totalAvail.getHorizon() * 1.2;
    double astep = (ah - ZAFunction::minTaskLength) / 1000.0;
    if (astep == 0) astep = 1;
    for (double a = ZAFunction::minTaskLength; a < ah; a += astep) {
        double maxAvailBeforeIt = maxAvail.getSlowness(a);
        double totalAvailBeforeIt = maxAvailBeforeIt - privateData.totalAvail.getSlowness(a);
        double aggrAvailBeforeIt = maxAvailBeforeIt - aggrAvail.getSlowness(a);
        double accuracy = totalAvailBeforeIt > 0.0 ? (aggrAvailBeforeIt * 100.0) / totalAvailBeforeIt : 100.0;
        if (totalAvailBeforeIt + 1 < aggrAvailBeforeIt)
            LogMsg("test", ERROR) << "total availability is lower than aggregated... (" << totalAvailBeforeIt << " < " << aggrAvailBeforeIt << ')';
        meanAccuracy += (prevAccuracy + accuracy) * (a - prevA);
        prevAccuracy = accuracy;
        prevA = a;
    }
    meanAccuracy /= 2.0 * (ah - ZAFunction::minTaskLength);

    results["M"].value(totalMem).value(minMem).value(aggrMem).value((aggrMem - minMem) * 100.0 / (totalMem - minMem));
    results["D"].value(totalDisk).value(minDisk).value(aggrDisk).value((aggrDisk - minDisk) * 100.0 / (totalDisk - minDisk));
    results["Z"].value(0.0).value(0.0).value(0.0).value(meanAccuracy);
}


AggregationTest & AggregationTest::getInstance() {
    static AggregationTestImpl<FSPAvailabilityInformation> instance;
    return instance;
}
