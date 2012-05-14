/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
 *
 *  This file is part of PeerComp.
 *
 *  PeerComp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  PeerComp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PeerComp; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <boost/test/unit_test.hpp>
#include "CheckMsg.hpp"
#include "AggregationTest.hpp"
#include "QueueBalancingInfo.hpp"
using namespace boost;


BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)

/// TaskEventMsg
BOOST_AUTO_TEST_CASE(qbiMsg) {
    // Ctor
    QueueBalancingInfo e;
    shared_ptr<QueueBalancingInfo> p;

    // TODO: Check other things

    CheckMsgMethod::check(e, p);
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor


template<> struct Priv<QueueBalancingInfo> {
    Duration maxQueue;
    Duration totalQueue;
};


static Time reference = Time::getCurrentTime();


template<> shared_ptr<QueueBalancingInfo> AggregationTest<QueueBalancingInfo>::createInfo(const AggregationTest::Node & n) {
    static const int min_time = 0;
    static const int max_time = 2000;
    static const int step_time = 1;
    shared_ptr<QueueBalancingInfo> result(new QueueBalancingInfo);
    Duration q((double)uniform(min_time, max_time, step_time));
    result->addQueueEnd(n.mem, n.disk, n.power, reference + q);
    totalInfo->addQueueEnd(n.mem, n.disk, n.power, reference + q);
    if (privateData.maxQueue < q)
        privateData.maxQueue = q;
    privateData.totalQueue += q;
    return result;
}


BOOST_AUTO_TEST_SUITE(Per)   // Performance test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(qbiAggr) {
    ofstream ofmd("aqbi_test_mem_disk_power.stat");
    int numClusters[] = { 16, 27, 81, 125, 256 };

    AggregationTest<QueueBalancingInfo> t;
    for (int i = 0; i < 17; i++) {
        for (int j = 0; j < 5; j++) {
            QueueBalancingInfo::setNumClusters(numClusters[j]);
            ofmd << "# " << numClusters[j] << " clusters" << endl;
            list<QueueBalancingInfo::MDPTCluster *> clusters;
            TaskDescription dummy;
            dummy.setMaxMemory(0);
            dummy.setMaxDisk(0);
            dummy.setLength(1);
            dummy.setDeadline(Time::getCurrentTime() + Duration(10000.0));
            shared_ptr<QueueBalancingInfo> result = t.test(i);
            result->getAvailability(clusters, dummy);
            // Do not calculate total information and then aggregate, it is not very useful
            unsigned long int aggrMem = 0, aggrDisk = 0, aggrPower = 0;
            Duration aggrQueue;
            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            unsigned long int minPower = t.getNumNodes() * t.min_power;
            Duration maxQueue = t.getPrivateData().maxQueue * t.getNumNodes();
            Duration totalQueue = maxQueue - t.getPrivateData().totalQueue;
            for (list<QueueBalancingInfo::MDPTCluster *>::iterator it = clusters.begin(); it != clusters.end(); it++) {
                aggrMem += (unsigned long int)(*it)->minM * (*it)->value;
                aggrDisk += (unsigned long int)(*it)->minD * (*it)->value;
                aggrPower += (unsigned long int)(*it)->minP * (*it)->value;
                aggrQueue += (t.getPrivateData().maxQueue - ((*it)->maxT - reference)) * (*it)->value;
            }
            LogMsg("Test.RI", INFO) << t.getNumNodes() << " nodes, " << " nc. " << numClusters[j] << ": min/mean/max " << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()
            << " mem " << aggrMem << " / " << t.getTotalMem() << " = " << (aggrMem * 100.0 / t.getTotalMem()) << "%"
            << " disk " << aggrDisk << " / " << t.getTotalDisk() << " = " << (aggrDisk * 100.0 / t.getTotalDisk()) << "%"
            << " power " << aggrPower << " / " << t.getTotalPower() << " = " << (aggrPower * 100.0 / t.getTotalPower()) << "%"
            << " queue " << aggrQueue.seconds() << " / " << totalQueue.seconds() << " = " << (aggrQueue.seconds() * 100.0 / totalQueue.seconds()) << "%";

            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalPower() << ',' << minPower << ',' << aggrPower << ',' << (aggrPower * 100.0 / t.getTotalPower()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << totalQueue.seconds() << ',' << maxQueue.seconds() << ',' << aggrQueue.seconds() << ',' << (aggrQueue.seconds() * 100.0 / totalQueue.seconds()) << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Per
