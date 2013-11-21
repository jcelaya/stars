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
#include "MMPAvailabilityInformation.hpp"


template<> struct Priv<MMPAvailabilityInformation> {
    Duration maxQueue;
    Duration totalQueue;
};


static Time reference = Time::getCurrentTime();


template<> AggregationTestImpl<MMPAvailabilityInformation>::AggregationTestImpl() : AggregationTest("mmp_mem_disk_power_queue.stat", 2) {
}


template<> std::shared_ptr<MMPAvailabilityInformation> AggregationTestImpl<MMPAvailabilityInformation>::createInfo(const AggregationTestImpl::Node & n) {
    static const int min_time = 0;
    static const int max_time = 2000;
    static const int step_time = 1;
    std::shared_ptr<MMPAvailabilityInformation> result(new MMPAvailabilityInformation);
    Duration q((double)floor(boost::random::uniform_int_distribution<>(min_time, max_time)(gen.getGenerator()) / step_time) * step_time);
    result->setQueueEnd(n.mem, n.disk, n.power, reference + q);
    if (privateData.maxQueue < q)
        privateData.maxQueue = q;
    privateData.totalQueue += q;
    return result;
}


template<> void AggregationTestImpl<MMPAvailabilityInformation>::computeResults(const std::shared_ptr<MMPAvailabilityInformation> & summary) {
    list<MMPAvailabilityInformation::MDPTCluster *> clusters;
    TaskDescription dummy;
    dummy.setMaxMemory(0);
    dummy.setMaxDisk(0);
    dummy.setLength(1);
    dummy.setDeadline(Time::getCurrentTime() + Duration(10000.0));
    summary->getAvailability(clusters, dummy);
    unsigned long int minMem = nodes.size() * min_mem;
    unsigned long int minDisk = nodes.size() * min_disk;
    unsigned long int minPower = nodes.size() * min_power;
    Duration maxQueue = privateData.maxQueue * nodes.size();
    Duration totalQueue = maxQueue - privateData.totalQueue;
    unsigned long int aggrMem = 0, aggrDisk = 0, aggrPower = 0;
    Duration aggrQueue;

    for (auto & u : clusters) {
        aggrMem += u->getTotalMemory();
        aggrDisk += u->getTotalDisk();
        aggrPower += u->getTotalSpeed();
        aggrQueue += privateData.maxQueue * u->getValue() - u->getTotalQueue(reference);
    }

    results["M"].value(totalMem).value(minMem).value(aggrMem).value((aggrMem - minMem) * 100.0 / (totalMem - minMem));
    results["D"].value(totalDisk).value(minDisk).value(aggrDisk).value((aggrDisk - minDisk) * 100.0 / (totalDisk - minDisk));
    results["S"].value(totalPower).value(minPower).value(aggrPower).value((aggrPower - minPower) * 100.0 / (totalPower - minPower));
    results["Q"].value(totalQueue.seconds()).value(maxQueue.seconds()).value(aggrQueue.seconds())
            .value(aggrQueue.seconds() * 100.0 / totalQueue.seconds());
}


AggregationTest & AggregationTest::getInstance() {
    static AggregationTestImpl<MMPAvailabilityInformation> instance;
    return instance;
}
