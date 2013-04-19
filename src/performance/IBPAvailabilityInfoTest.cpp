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

#include "AggregationTest.hpp"
#include "IBPAvailabilityInformation.hpp"
#include "Logger.hpp"


template<> struct Priv<IBPAvailabilityInformation> {};


template<> shared_ptr<IBPAvailabilityInformation> AggregationTest<IBPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    shared_ptr<IBPAvailabilityInformation> result(new IBPAvailabilityInformation);
    result->addNode(n.mem, n.disk);
    return result;
}


void performanceTest(const std::vector<int> & numClusters, int levels) {
    ofstream ofmd("ibp_mem_disk.stat");

    for (int j = 0; j < numClusters.size(); ++j) {
        IBPAvailabilityInformation::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        LogMsg("Progress", WARN) << "Testing with " << numClusters[j] << " clusters";
        AggregationTest<IBPAvailabilityInformation> t;
        for (int i = 0; i < levels; ++i) {
            LogMsg("Progress", WARN) << i << " levels";
            list<IBPAvailabilityInformation::MDCluster *> clusters;
            TaskDescription dummy;
            dummy.setMaxMemory(0);
            dummy.setMaxDisk(0);
            shared_ptr<IBPAvailabilityInformation> result = t.test(i);
            result->getAvailability(clusters, dummy);
            // Do not calculate total information and then aggregate, it is not very useful
            unsigned long int aggrMem = 0, aggrDisk = 0;
            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            for (list<IBPAvailabilityInformation::MDCluster *>::iterator it = clusters.begin(); it != clusters.end(); it++) {
                aggrMem += (unsigned long int)(*it)->minM * (*it)->value;
                aggrDisk += (unsigned long int)(*it)->minD * (*it)->value;
            }

            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "M," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << aggrMem << ',' << ((aggrMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << endl;
            ofmd << "D," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << aggrDisk << ',' << ((aggrDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << endl;
            ofmd << "s," << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}
