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
#include "AggregationTest.hpp"
#include "DPAvailabilityInformation.hpp"


template<> struct Priv<DPAvailabilityInformation> {
    stars::LDeltaFunction totalAvail;
    stars::LDeltaFunction minAvail;
};


template<> AggregationTestImpl<DPAvailabilityInformation>::AggregationTestImpl() : AggregationTest("dp_mem_disk_avail.stat", 2) {
    stars::ClusteringList<DPAvailabilityInformation::MDFCluster>::setDistVectorSize(20);
    stars::LDeltaFunction::setNumPieces(10);
}


template<> std::shared_ptr<DPAvailabilityInformation> AggregationTestImpl<DPAvailabilityInformation>::createInfo(const AggregationTestImpl::Node & n) {
    std::shared_ptr<DPAvailabilityInformation> result(new DPAvailabilityInformation);
    result->addNode(n.mem, n.disk, n.power, gen.createRandomQueue(n.power));
    const stars::LDeltaFunction & minA = result->getSummary().front().minA;
    if (privateData.minAvail.getSlope() == 0.0)
        privateData.minAvail = minA;
    else
        privateData.minAvail.min(privateData.minAvail, minA);
    privateData.totalAvail.lc(privateData.totalAvail, minA, 1.0, 1.0);
    return result;
}


template<> void AggregationTestImpl<DPAvailabilityInformation>::computeResults(const std::shared_ptr<DPAvailabilityInformation> & summary) {
    static Time refTime = Time::getCurrentTime();
    static stars::LDeltaFunction dummy;
    unsigned long int minMem = nodes.size() * min_mem;
    unsigned long int minDisk = nodes.size() * min_disk;
    stars::LDeltaFunction minAvail, aggrAvail;
    minAvail.lc(privateData.minAvail, dummy, nodes.size(), 1.0);

    unsigned long int aggrMem = 0, aggrDisk = 0;
    double meanAccuracy = 0.0;
    const stars::ClusteringList<DPAvailabilityInformation::MDFCluster> & clusters = summary->getSummary();
    for (auto & u : clusters) {
        aggrMem += (unsigned long int)u.minM * u.value;
        aggrDisk += (unsigned long int)u.minD * u.value;
        aggrAvail.lc(aggrAvail, u.minA, 1.0, u.value);
    }
    Time max = aggrAvail.getHorizon();
    if (max < privateData.totalAvail.getHorizon())
        max = privateData.totalAvail.getHorizon();
    if (max < minAvail.getHorizon())
        max = minAvail.getHorizon();
    max += (max - refTime) * 1.2;
    double prevAccuracy = 0.0;
    Time prevTime = refTime;
    Duration step = (max - refTime) * 0.001;
    for (Time i = refTime; i < max; i += step) {
        double minAvailBeforeIt = minAvail.getAvailabilityBefore(i);
        double totalAvailBeforeIt = privateData.totalAvail.getAvailabilityBefore(i) - minAvailBeforeIt;
        double aggrAvailBeforeIt = aggrAvail.getAvailabilityBefore(i) - minAvailBeforeIt;
        double accuracy = totalAvailBeforeIt > 0.0 ? (aggrAvailBeforeIt * 100.0) / totalAvailBeforeIt : 100.0;
        if (totalAvailBeforeIt + 1 < aggrAvailBeforeIt)
            Logger::msg("test", ERROR, "total availability is lower than aggregated... (", totalAvailBeforeIt, " < ", aggrAvailBeforeIt, ')');
        meanAccuracy += (prevAccuracy + accuracy) * (i - prevTime).seconds();
        prevAccuracy = accuracy;
        prevTime = i;
    }
    meanAccuracy /= 2.0 * (max - refTime).seconds();

    results["M"].value(totalMem).value(minMem).value(aggrMem).value((aggrMem - minMem) * 100.0 / (totalMem - minMem));
    results["D"].value(totalDisk).value(minDisk).value(aggrDisk).value((aggrDisk - minDisk) * 100.0 / (totalDisk - minDisk));
    results["A"].value(0.0).value(0.0).value(0.0).value(meanAccuracy);
}


AggregationTest & AggregationTest::getInstance() {
    static AggregationTestImpl<DPAvailabilityInformation> instance;
    return instance;
}
