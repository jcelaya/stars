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
//using namespace boost;


template<> struct Priv<DPAvailabilityInformation> {
    DPAvailabilityInformation::ATFunction totalAvail;
    DPAvailabilityInformation::ATFunction minAvail;
};


namespace {
    Time refTime;

    void createRandomLAF(double power, boost::random::mt19937 & gen, list<Time> & result) {
        Time next = refTime, h = refTime + Duration(100000.0);

        // Add a random number of tasks, with random length
        while(boost::random::uniform_int_distribution<>(1, 3)(gen) != 1) {
            // Tasks of 5-60 minutes on a 1000 MIPS computer
            unsigned long int length = boost::random::uniform_int_distribution<>(300000, 3600000)(gen);
            next += Duration(length / power);
            result.push_back(next);
            // Similar time for holes
            unsigned long int nextHole = boost::random::uniform_int_distribution<>(300000, 3600000)(gen);
            next += Duration(nextHole / power);
            result.push_back(next);
        }
        if (!result.empty()) {
            // set a good horizon
            if (next < h) result.back() = h;
        }
    }
}


template<> AggregationTestImpl<DPAvailabilityInformation>::AggregationTestImpl() : AggregationTest("dp_mem_disk_avail.stat", 2) {
    stars::ClusteringList<DPAvailabilityInformation::MDFCluster>::setDistVectorSize(20);
    DPAvailabilityInformation::setNumRefPoints(10);
}


template<> boost::shared_ptr<DPAvailabilityInformation> AggregationTestImpl<DPAvailabilityInformation>::createInfo(const AggregationTestImpl::Node & n) {
    boost::shared_ptr<DPAvailabilityInformation> result(new DPAvailabilityInformation);
    list<Time> q;
    createRandomLAF(n.power, gen, q);
    result->addNode(n.mem, n.disk, n.power, q);
    const DPAvailabilityInformation::ATFunction & minA = result->getSummary().front().minA;
    if (privateData.minAvail.getSlope() == 0.0)
        privateData.minAvail = minA;
    else
        privateData.minAvail.min(privateData.minAvail, minA);
    privateData.totalAvail.lc(privateData.totalAvail, minA, 1.0, 1.0);
    return result;
}


template<> void AggregationTestImpl<DPAvailabilityInformation>::computeResults(const boost::shared_ptr<DPAvailabilityInformation> & summary) {
    static DPAvailabilityInformation::ATFunction dummy;
    unsigned long int minMem = nodes.size() * min_mem;
    unsigned long int minDisk = nodes.size() * min_disk;
    DPAvailabilityInformation::ATFunction minAvail, aggrAvail;
    minAvail.lc(privateData.minAvail, dummy, nodes.size(), 1.0);

    unsigned long int aggrMem = 0, aggrDisk = 0;
    double meanAccuracy = 0.0;
    const stars::ClusteringList<DPAvailabilityInformation::MDFCluster> & clusters = summary->getSummary();
    for (auto & u : clusters) {
        aggrMem += (unsigned long int)u.minM * u.value;
        aggrDisk += (unsigned long int)u.minD * u.value;
        aggrAvail.lc(aggrAvail, u.minA, 1.0, u.value);
    }
    list<Time> p;
    for (auto & i: aggrAvail.getPoints())
        p.push_back(i.first);
    for (auto & i: privateData.totalAvail.getPoints())
        p.push_back(i.first);
    for (auto & i: minAvail.getPoints())
        p.push_back(i.first);
    p.sort();
    p.erase(std::unique(p.begin(), p.end()), p.end());
    // TODO: The accuracy is not linear...
    double prevAccuracy = 0.0;
    Time prevTime = refTime;
    // Approximate the accuracy to linear...
    for (auto & i: p) {
        double minAvailBeforeIt = minAvail.getAvailabilityBefore(i);
        double totalAvailBeforeIt = privateData.totalAvail.getAvailabilityBefore(i)- minAvailBeforeIt;
        double aggrAvailBeforeIt = aggrAvail.getAvailabilityBefore(i)- minAvailBeforeIt;
        double accuracy = totalAvailBeforeIt > minAvailBeforeIt ? (aggrAvailBeforeIt * 100.0) / totalAvailBeforeIt : 0.0;
        if (totalAvailBeforeIt + 1 < aggrAvailBeforeIt)
            LogMsg("test", ERROR) << "total availability is lower than aggregated... (" << totalAvailBeforeIt << " < " << aggrAvailBeforeIt << ')';
        meanAccuracy += (prevAccuracy + accuracy) * (i - prevTime).seconds();
        prevAccuracy = accuracy;
        prevTime = i;
    }
    meanAccuracy /= 2.0 * (p.back() - refTime).seconds();

    results["M"].value(totalMem).value(minMem).value(aggrMem).value((aggrMem - minMem) * 100.0 / (totalMem - minMem));
    results["D"].value(totalDisk).value(minDisk).value(aggrDisk).value((aggrDisk - minDisk) * 100.0 / (totalDisk - minDisk));
    results["A"].value(0.0).value(0.0).value(0.0).value(meanAccuracy);
}


AggregationTest & AggregationTest::getInstance() {
    static AggregationTestImpl<DPAvailabilityInformation> instance;
    return instance;
}
