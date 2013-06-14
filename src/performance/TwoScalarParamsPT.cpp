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
#include "TwoSPAvailInfo.hpp"


unsigned int TwoSPAvailInfo::numClusters = 4;
unsigned int TwoSPAvailInfo::numIntervals = 2;


template<> struct Priv<TwoSPAvailInfo> {};


template<> boost::shared_ptr<TwoSPAvailInfo> AggregationTest<TwoSPAvailInfo>::createInfo(const AggregationTest::Node & n) {
    boost::shared_ptr<TwoSPAvailInfo> result(new TwoSPAvailInfo);
    result->addNode(n.mem, n.disk);
    return result;
}


void performanceTest(const std::vector<int> & numClusters, int levels) {
    ofstream ofmd("test_mem_disk.stat");

    for (int j = 0; j < numClusters.size(); ++j) {
        TwoSPAvailInfo::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        AggregationTest<TwoSPAvailInfo> t;
        for (int currentLevel = 0; currentLevel < levels; ++currentLevel) {
            list<TwoSPAvailInfo::Cluster *> clusters;
            TaskDescription dummy;
            dummy.setMaxMemory(0);
            dummy.setMaxDisk(0);
            boost::shared_ptr<TwoSPAvailInfo> result = t.test(currentLevel);
            result->getAvailability(clusters, dummy);
            // Do not calculate total information and then aggregate, it is not very useful
            unsigned long int aggrMem = 0, aggrDisk = 0;
            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            for (list<TwoSPAvailInfo::Cluster *>::iterator it = clusters.begin(); it != clusters.end(); it++) {
                aggrMem += (unsigned long int)(*it)->getTotalMemory();
                aggrDisk += (unsigned long int)(*it)->getTotalDisk();
            }
            ofmd << "# " << (currentLevel + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "M," << (currentLevel + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << (minMem * 100.0 / t.getTotalMem()) << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << "D," << (currentLevel + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << (minDisk * 100.0 / t.getTotalDisk()) << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << "s," << (currentLevel + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}
