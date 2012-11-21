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

#include <list>
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <fstream>
#include "CheckMsg.hpp"
#include "AggregationTest.hpp"
#include "DPAvailabilityInformation.hpp"
#include "TestHost.hpp"
using namespace std;
using namespace boost;


BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)

/// TaskEventMsg
BOOST_AUTO_TEST_CASE(tciMsg) {
    TestHost::getInstance().reset();

    // Ctor
    DPAvailabilityInformation e;
    shared_ptr<DPAvailabilityInformation> p;

    // TODO: Check other things

    CheckMsgMethod::check(e, p);
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor


template<> struct Priv<DPAvailabilityInformation> {
    Time ref;
    DPAvailabilityInformation::ATFunction totalAvail;
    DPAvailabilityInformation::ATFunction minAvail;
};


namespace {
    list<Time> createRandomLAF(double power, Time ct) {
        Time next = ct, h = ct + Duration(100000.0);
        list<Time> result;

        // Add a random number of tasks, with random length
        while(AggregationTest<DPAvailabilityInformation>::uniform(1, 3) != 1) {
            // Tasks of 5-60 minutes on a 1000 MIPS computer
            unsigned long int length = AggregationTest<DPAvailabilityInformation>::uniform(300000, 3600000);
            next += Duration(length / power);
            result.push_back(next);
            // Similar time for holes
            unsigned long int nextHole = AggregationTest<DPAvailabilityInformation>::uniform(300000, 3600000);
            next += Duration(nextHole / power);
            result.push_back(next);
        }
        if (!result.empty()) {
            // set a good horizon
            if (next < h) result.back() = h;
        }

        return result;
    }


    string plot(DPAvailabilityInformation::ATFunction & f) {
        ostringstream os;
        if (f.getPoints().empty()) {
            os << "0,0" << endl;
            os << "100000000000," << (unsigned long)(f.getSlope() * 100000.0) << endl;
        } else {
            for (vector<pair<Time, uint64_t> >::const_iterator it = f.getPoints().begin(); it != f.getPoints().end(); it++)
                os << it->first.getRawDate() << ',' << it->second << endl;
        }
        return os.str();
    }
}


template<> shared_ptr<DPAvailabilityInformation> AggregationTest<DPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    shared_ptr<DPAvailabilityInformation> result(new DPAvailabilityInformation);
    list<Time> q = createRandomLAF(n.power, privateData.ref);
    result->addNode(n.mem, n.disk, n.power, q);
    totalInfo->addNode(n.mem, n.disk, n.power, q);
    const DPAvailabilityInformation::ATFunction & minA = result->getSummary()[0].minA;
    if (privateData.minAvail.getSlope() == 0.0)
        privateData.minAvail = minA;
    else
        privateData.minAvail.min(privateData.minAvail, minA);
    privateData.totalAvail.lc(privateData.totalAvail, minA, 1.0, 1.0);
    return result;
}


/// Test Cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(ATFunction) {
    TestHost::getInstance().reset();

    Time ct = Time::getCurrentTime();
    Time h = ct + Duration(100000.0);
    DPAvailabilityInformation::setNumRefPoints(8);
    list<Time> points;
    points.push_back(ct + Duration(10.0));
    points.push_back(ct + Duration(15.0));
    points.push_back(ct + Duration(17.3));
    points.push_back(ct + Duration(21.8));
    points.push_back(ct + Duration(33.0));
    points.push_back(ct + Duration(34.0));
    DPAvailabilityInformation f;
    f.addNode(100, 200, 35, points);
    //TimeConstraintInfo::ATFunction f(35, points);
    LogMsg("Test.RI", INFO) << "Random Function: " << f;

    // Min/max and sum of several functions
    LogMsg("Test.RI", INFO) << "";
    ofstream of("af_test.stat");
    for (int i = 0; i < 500; i++) {
        LogMsg("Test.RI", INFO) << "Functions " << i;

        double f11power = AggregationTest<DPAvailabilityInformation>::uniform(1000, 3000, 200),
               f12power = AggregationTest<DPAvailabilityInformation>::uniform(1000, 3000, 200),
               f13power = AggregationTest<DPAvailabilityInformation>::uniform(1000, 3000, 200),
               f21power = AggregationTest<DPAvailabilityInformation>::uniform(1000, 3000, 200),
               f22power = AggregationTest<DPAvailabilityInformation>::uniform(1000, 3000, 200);
               DPAvailabilityInformation::ATFunction
               f11(f11power, createRandomLAF(f11power, ct)),
               f12(f12power, createRandomLAF(f12power, ct)),
               f13(f13power, createRandomLAF(f13power, ct)),
               f21(f21power, createRandomLAF(f21power, ct)),
               f22(f22power, createRandomLAF(f22power, ct));
               DPAvailabilityInformation::ATFunction min, max;
               min.min(f11, f12);
               min.min(min, f13);
               min.min(min, f21);
               min.min(min, f22);
               max.max(f11, f12);
               max.max(max, f13);
               max.max(max, f21);
               max.max(max, f22);

               // Join f11 with f12
               DPAvailabilityInformation::ATFunction f112;
               double accumAsq112 = f112.minAndLoss(f11, f12, 1, 1, DPAvailabilityInformation::ATFunction(), DPAvailabilityInformation::ATFunction(), ct, h);
               BOOST_CHECK_GE(accumAsq112 * 1.0001, f112.sqdiff(f11, ct, h) + f112.sqdiff(f12, ct, h));
               DPAvailabilityInformation::ATFunction accumAln112;
               accumAln112.max(f11, f12);
               BOOST_CHECK_CLOSE(accumAsq112, f11.sqdiff(f12, ct, h), 0.0001);

               // join f112 with f13, and that is f1
               DPAvailabilityInformation::ATFunction f1;
               double accumAsq1 = f1.minAndLoss(f112, f13, 2, 1, accumAln112, DPAvailabilityInformation::ATFunction(), ct, h) + accumAsq112;
               BOOST_CHECK_GE(accumAsq1 * 1.0001, f1.sqdiff(f11, ct, h) + f1.sqdiff(f12, ct, h) + f1.sqdiff(f13, ct, h));
               DPAvailabilityInformation::ATFunction accumAln1;
               accumAln1.max(accumAln112, f13);

               // join f21 with f22, and that is f2
               DPAvailabilityInformation::ATFunction f2;
               double accumAsq2 = f2.minAndLoss(f21, f22, 1, 1, DPAvailabilityInformation::ATFunction(), DPAvailabilityInformation::ATFunction(), ct, h);
               BOOST_CHECK_GE(accumAsq2 * 1.0001, f2.sqdiff(f21, ct, h) + f2.sqdiff(f22, ct, h));
               DPAvailabilityInformation::ATFunction accumAln2;
               accumAln2.max(f21, f22);

               // join f1 with f2, and that is f
               DPAvailabilityInformation::ATFunction f;
               double accumAsq = f.minAndLoss(f1, f2, 3, 2, accumAln1, accumAln2, ct, h) + accumAsq1 + accumAsq2;
               BOOST_CHECK_GE(accumAsq * 1.0001, f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h));
               DPAvailabilityInformation::ATFunction accumAln;
               accumAln.max(accumAln1, accumAln2);

               // Print functions
               of << "# Functions " << i << endl;
               //              of << "# minloss(a, b) = " << loss << "; a.loss(b) = " << a.loss(b, ct, ref) << "; b.loss(a) = " << b.loss(a, ct, ref) << "; a.loss(min) = " << a.loss(r, ct, ref)
               //                              << "; b.loss(min) = " << b.loss(r, ct, ref) << "; max.loss(min) = " << t.loss(r, ct, ref) << "; min.loss(max) = " << r.loss(t, ct, ref) << endl;
               //              // Take two random values j and k
               //              unsigned int j = uniform(2, 30, 1), k = uniform(2, 30, 1);
               //              of << "# " << j << "a.loss(min) = " << (j * a.loss(r, ct, ref)) << "; " << k << "b.loss(min) = " << (k * b.loss(r, ct, ref))
               //                              << "; sum " << (j * a.loss(r, ct, ref) + k * b.loss(r, ct, ref)) << "; a.loss(b, " << j << ", " << k << ") = " << min.minAndLoss(a, b, j, k, ct, ref) << endl;
               of << "# f11: " << f11 << endl << plot(f11) << endl;
               of << "# f12: " << f12 << endl << plot(f12) << endl;
               of << "# f112: " << f112 << endl << "# accumAsq112 " << accumAsq112 << " =? " << (f112.sqdiff(f11, ct, h) + f112.sqdiff(f12, ct, h)) << endl << plot(f112) << endl;
               of << "# accumAln112: " << accumAln112 << endl << plot(accumAln112) << endl;
               of << "# f13: " << f13 << endl << plot(f13) << endl;
               of << "# f1: " << f1 << endl << "# accumAsq1 " << accumAsq1 << " =? " << (f1.sqdiff(f11, ct, h) + f1.sqdiff(f12, ct, h) + f1.sqdiff(f13, ct, h)) << endl << plot(f1) << endl;
               of << "# accumAln1: " << accumAln1 << endl << plot(accumAln1) << endl;
               of << "# f21: " << f21 << endl << plot(f21) << endl;
               of << "# f22: " << f22 << endl << plot(f22) << endl;
               of << "# f2: " << f2 << endl << "# accumAsq2 " << accumAsq2 << " =? " << (f2.sqdiff(f21, ct, h) + f2.sqdiff(f22, ct, h)) << endl << plot(f2) << endl;
               of << "# accumAln2: " << accumAln2 << endl << plot(accumAln2) << endl;
               of << "# f: " << f << endl << "# accumAsq " << accumAsq << " =? " << (f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h)) << endl << plot(f) << endl;
               of << "# accumAln: " << accumAln << endl << plot(accumAln) << endl;
               accumAsq += f.reduceMin(5, accumAln, ct, h);
               BOOST_CHECK_GE(accumAsq * 1.0001, f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h));
               of << "# f reduced: " << f << endl << "# accumAsq " << accumAsq << " =? " << (f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h)) << endl << plot(f) << endl;
               accumAln.reduceMax(ct, h);
               of << "# accumAln reduced: " << accumAln << endl << plot(accumAln) << endl;
               of << endl;
    }
    of.close();
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor


BOOST_AUTO_TEST_SUITE(Per)   // Performance test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(tciAggr) {
    TestHost::getInstance().reset();
    int numClusters[] = { 8, 64, 225 };

    Time ct = Time::getCurrentTime();
    ClusteringVector<DPAvailabilityInformation::MDFCluster>::setDistVectorSize(20);
    unsigned int numpoints = 10;
    DPAvailabilityInformation::setNumRefPoints(numpoints);
    ofstream off("atci_test_function.stat"), ofmd("atci_test_mem_disk.stat");
    DPAvailabilityInformation::ATFunction dummy;

    for (int j = 0; j < 3; j++) {
        DPAvailabilityInformation::setNumClusters(numClusters[j]);
        off << "# " << numClusters[j] << " clusters" << endl;
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        AggregationTest<DPAvailabilityInformation> t;
        t.getPrivateData().ref = ct;
        for (int i = 0; i < 17; i++) {
            shared_ptr<DPAvailabilityInformation> result = t.test(i);

            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            DPAvailabilityInformation::ATFunction minAvail, aggrAvail, treeAvail;
            DPAvailabilityInformation::ATFunction & totalAvail = const_cast<DPAvailabilityInformation::ATFunction &>(t.getPrivateData().totalAvail);
            minAvail.lc(t.getPrivateData().minAvail, dummy, t.getNumNodes(), 1.0);

            unsigned long int aggrMem = 0, aggrDisk = 0;
            {
                shared_ptr<DPAvailabilityInformation> totalInformation = t.getTotalInformation();
                const ClusteringVector<DPAvailabilityInformation::MDFCluster> & clusters = totalInformation->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const DPAvailabilityInformation::MDFCluster & u = clusters[j];
                    aggrMem += (unsigned long int)u.minM * u.value;
                    aggrDisk += (unsigned long int)u.minD * u.value;
                    aggrAvail.lc(aggrAvail, u.minA, 1.0, u.value);
                }
            }

            unsigned long int treeMem = 0, treeDisk = 0;
            {
                const ClusteringVector<DPAvailabilityInformation::MDFCluster> & clusters = result->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const DPAvailabilityInformation::MDFCluster & u = clusters[j];
                    BOOST_CHECK(u.minA.getPoints().size() <= numpoints);
                    BOOST_CHECK(u.accumMaxA.getPoints().size() <= numpoints);
                    treeMem += (unsigned long int)u.minM * u.value;
                    treeDisk += (unsigned long int)u.minD * u.value;
                    treeAvail.lc(treeAvail, u.minA, 1.0, u.value);
                }
            }

            LogMsg("Test.RI", INFO) << t.getNumNodes() << " nodes, " << numClusters[j] << " s.f., "
                    << t.getMeanTime().total_microseconds() << " us/msg, "
                    << "min/mean/max size " << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()
                    << " mem " << (treeMem - minMem) << '/' << (t.getTotalMem() - minMem) << '(' << ((treeMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "%)"
                    << " disk " << (treeDisk - minDisk) << '/' << (t.getTotalDisk() - minDisk) << '(' << ((treeDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "%)";
                    //<< " avail " << deltaAggrAvail << '/' << deltaTotalAvail;
            LogMsg("Test.RI", INFO) << "Full aggregation: "
                    << " mem " << (aggrMem - minMem) << '/' << (t.getTotalMem() - minMem) << '(' << ((aggrMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "%)"
                    << " disk " << (aggrDisk - minDisk) << '/' << (t.getTotalDisk() - minDisk) << '(' << ((aggrDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "%)";
                    //<< " avail " << deltaAggrAvail << '/' << deltaTotalAvail;
            list<Time> p;
            for (vector<pair<Time, uint64_t> >::const_iterator it = aggrAvail.getPoints().begin(); it != aggrAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, uint64_t> >::const_iterator it = treeAvail.getPoints().begin(); it != treeAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, uint64_t> >::const_iterator it = totalAvail.getPoints().begin(); it != totalAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, uint64_t> >::const_iterator it = minAvail.getPoints().begin(); it != minAvail.getPoints().end(); it++)
                p.push_back(it->first);
            p.sort();
            off << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << (minMem * 100.0 / t.getTotalMem()) << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << ',' << treeMem << ',' << (treeMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << (minDisk * 100.0 / t.getTotalDisk()) << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << ',' << treeDisk << ',' << (treeDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            double lastTime = -1.0;
            for (list<Time>::iterator it = p.begin(); it != p.end(); it++) {
                unsigned long t = totalAvail.getAvailabilityBefore(*it),
                             a = aggrAvail.getAvailabilityBefore(*it),
                             at = treeAvail.getAvailabilityBefore(*it),
                             min = minAvail.getAvailabilityBefore(*it);
                             double time = floor((*it - ct).seconds() * 1000.0) / 1000.0;
                             if (lastTime != time) {
                                 off << time << ',' << t
                                 << ',' << min << ',' << (t == 0 ? 100 : min * 100.0 / t)
                                 << ',' << a << ',' << (t == 0 ? 100 : a * 100.0 / t)
                                 << ',' << at << ',' << (t == 0 ? 100 : at * 100.0 / t) << endl;
                                 lastTime = time;
                             }
            }
            off << endl;
            ofmd << endl;
        }
        off << endl;
        ofmd << endl;
    }
    off.close();
    ofmd.close();
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Per
