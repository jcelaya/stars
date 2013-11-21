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
#include "IBPAvailabilityInformation.hpp"
#include "Logger.hpp"


template<> struct Priv<IBPAvailabilityInformation> {};


template<> AggregationTestImpl<IBPAvailabilityInformation>::AggregationTestImpl() : AggregationTest("ibp_mem_disk.stat", 2) {
}


template<> std::shared_ptr<IBPAvailabilityInformation> AggregationTestImpl<IBPAvailabilityInformation>::createInfo(const AggregationTestImpl::Node & n) {
    std::shared_ptr<IBPAvailabilityInformation> result(new IBPAvailabilityInformation);
    result->addNode(n.mem, n.disk);
    return result;
}


template<> void AggregationTestImpl<IBPAvailabilityInformation>::computeResults(const std::shared_ptr<IBPAvailabilityInformation> & summary) {
    list<IBPAvailabilityInformation::MDCluster *> clusters;
    TaskDescription dummy;
    dummy.setMaxMemory(0);
    dummy.setMaxDisk(0);
    summary->getAvailability(clusters, dummy);
    unsigned long int minMem = nodes.size() * min_mem;
    unsigned long int minDisk = nodes.size() * min_disk;
    unsigned long int aggrMem = 0, aggrDisk = 0;

    for (auto & u : clusters) {
        aggrMem += u->getTotalMemory();
        aggrDisk += u->getTotalDisk();
    }

    results["M"].value(totalMem).value(minMem).value(aggrMem).value((aggrMem - minMem) * 100.0 / (totalMem - minMem));
    results["D"].value(totalDisk).value(minDisk).value(aggrDisk).value((aggrDisk - minDisk) * 100.0 / (totalDisk - minDisk));
}


AggregationTest & AggregationTest::getInstance() {
    static AggregationTestImpl<IBPAvailabilityInformation> instance;
    return instance;
}
