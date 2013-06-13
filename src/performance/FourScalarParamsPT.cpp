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
#include "FourSPAvailInfo.hpp"
//using namespace boost;


unsigned int FourSPAvailInfo::numClusters = 16;
unsigned int FourSPAvailInfo::numIntervals = 2;


template<> struct Priv<FourSPAvailInfo> {
    Duration maxQueue;
    Duration totalQueue;
};


static Time reference = Time::getCurrentTime();


template<> boost::shared_ptr<FourSPAvailInfo> AggregationTest<FourSPAvailInfo>::createInfo(const AggregationTest::Node & n) {
    static const int min_time = 0;
    static const int max_time = 2000;
    boost::shared_ptr<FourSPAvailInfo> result(new FourSPAvailInfo);
    Duration q((double)boost::random::uniform_int_distribution<>(min_time, max_time)(gen));
    result->setQueueEnd(n.mem, n.disk, n.power, q);
    if (privateData.maxQueue < q)
        privateData.maxQueue = q;
    privateData.totalQueue += q;
    return result;
}


void performanceTest(const std::vector<int> & numClusters, int levels) {
    ofstream ofmd("fspi_test_mem_disk_power.stat");

    for (int j = 0; j < numClusters.size(); j++) {
        FourSPAvailInfo::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        AggregationTest<FourSPAvailInfo> t;
        for (int i = 0; i < levels; i++) {
            list<FourSPAvailInfo::Cluster *> clusters;
            TaskDescription dummy;
            dummy.setMaxMemory(0);
            dummy.setMaxDisk(0);
            dummy.setLength(1);
            dummy.setDeadline(Time::getCurrentTime() + Duration(10000.0));
            boost::shared_ptr<FourSPAvailInfo> result = t.test(i);
            result->getAvailability(clusters, dummy);
            // Do not calculate total information and then aggregate, it is not very useful
            unsigned long int aggrMem = 0, aggrDisk = 0, aggrPower = 0;
            Duration aggrQueue;
            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            unsigned long int minPower = t.getNumNodes() * t.min_power;
            Duration maxQueue = t.getPrivateData().maxQueue * t.getNumNodes();
            Duration totalQueue = maxQueue - t.getPrivateData().totalQueue;
            for (list<FourSPAvailInfo::Cluster *>::iterator it = clusters.begin(); it != clusters.end(); it++) {
                aggrMem += (*it)->getTotalMemory();
                aggrDisk += (*it)->getTotalDisk();
                aggrPower += (*it)->getTotalSpeed();
                aggrQueue += t.getPrivateData().totalQueue - (*it)->getTotalQueue();
            }

            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "M," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << "D," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << "S," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalPower() << ',' << minPower << ',' << aggrPower << ',' << (aggrPower * 100.0 / t.getTotalPower()) << endl;
            ofmd << "Q," << (i + 1) << ',' << numClusters[j] << ',' << totalQueue.seconds() << ',' << maxQueue.seconds() << ',' << aggrQueue.seconds() << ',' << (aggrQueue.seconds() * 100.0 / totalQueue.seconds()) << endl;
            ofmd << "s," << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}
