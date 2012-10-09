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

#include <boost/test/unit_test.hpp>
#include "CheckMsg.hpp"
#include "AggregationTest.hpp"
#include "BasicAvailabilityInfo.hpp"
using namespace boost;


BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)

/// TaskEventMsg
BOOST_AUTO_TEST_CASE(baiMsg) {
    // Ctor
    BasicAvailabilityInfo e;
    shared_ptr<BasicAvailabilityInfo> p;

    // TODO: Check other things

    CheckMsgMethod::check(e, p);
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor


template<> struct Priv<BasicAvailabilityInfo> {};


template<> shared_ptr<BasicAvailabilityInfo> AggregationTest<BasicAvailabilityInfo>::createInfo(const AggregationTest::Node & n) {
    shared_ptr<BasicAvailabilityInfo> result(new BasicAvailabilityInfo);
    result->addNode(n.mem, n.disk);
    totalInfo->addNode(n.mem, n.disk);
    return result;
}


BOOST_AUTO_TEST_SUITE(Per)   // Performance test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(baiAggr) {
    ofstream ofmd("abai_test_mem_disk.stat");
    int numClusters[] = { 9, 64, 225 };

    for (int j = 0; j < 3; ++j) {
        BasicAvailabilityInfo::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        AggregationTest<BasicAvailabilityInfo> t;
        for (int i = 0; i < 17; ++i) {
            list<BasicAvailabilityInfo::MDCluster *> clusters;
            TaskDescription dummy;
            dummy.setMaxMemory(0);
            dummy.setMaxDisk(0);
            shared_ptr<BasicAvailabilityInfo> result = t.test(i);
            result->getAvailability(clusters, dummy);
            // Do not calculate total information and then aggregate, it is not very useful
            unsigned long int aggrMem = 0, aggrDisk = 0;
            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            for (list<BasicAvailabilityInfo::MDCluster *>::iterator it = clusters.begin(); it != clusters.end(); it++) {
                aggrMem += (unsigned long int)(*it)->minM * (*it)->value;
                aggrDisk += (unsigned long int)(*it)->minD * (*it)->value;
            }
            LogMsg("Test.RI", INFO) << t.getNumNodes() << " nodes, " << numClusters[j] << " s.f., "
                    << t.getMeanTime().total_microseconds() << " us/msg, "
                    << "min/mean/max size " << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()
                    << " mem " << aggrMem << '/' << t.getTotalMem() << '(' << (aggrMem * 100.0 / t.getTotalMem()) << "%)"
                    << " disk " << aggrDisk << '/' << t.getTotalDisk() << '(' << (aggrDisk * 100.0 / t.getTotalDisk()) << "%)";

            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << (minMem * 100.0 / t.getTotalMem()) << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << (minDisk * 100.0 / t.getTotalDisk()) << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Per
