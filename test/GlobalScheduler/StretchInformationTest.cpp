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

#include <sstream>
#include <boost/test/unit_test.hpp>
#include "CheckMsg.hpp"
#include "TestHost.hpp"
#include "AggregationTest.hpp"
#include "../ExecutionManager/TestTask.hpp"
#include "StretchInformation.hpp"
#include "MinStretchScheduler.hpp"
using std::list;


template<> struct Priv<StretchInformation> {
    StretchInformation::HSWFunction totalAvail;
    StretchInformation::HSWFunction minAvail;
};


static double createRandomQueue(unsigned int maxmem, unsigned int maxdisk, double power, list<StretchInformation::AppDesc> & apps) {
    list<shared_ptr<Task> > result;
    Time now = TestHost::getInstance().getCurrentTime();

    // Add a random number of applications, with random length and number of tasks
    for (int appid = 0; AggregationTest<StretchInformation>::uniform(1, 3) != 1; ++appid) {
        double r = AggregationTest<StretchInformation>::uniform(-1000, 0);
        TaskDescription desc;
        // Applications between 1-4h on a 1000 MIPS computer
        int w = AggregationTest<StretchInformation>::uniform(600000, 14400000);
        desc.setNumTasks(AggregationTest<StretchInformation>::uniform(1, 10));
        desc.setLength(w / desc.getNumTasks());
        desc.setMaxMemory(AggregationTest<StretchInformation>::uniform(1, maxmem));
        desc.setMaxDisk(AggregationTest<StretchInformation>::uniform(1, maxdisk));
        TestHost::getInstance().setCurrentTime(now + Duration(r));
        for (unsigned int taskid = 0; taskid < desc.getNumTasks(); ++taskid)
            result.push_back(shared_ptr<Task>(new TestTask(CommAddress(), appid, taskid, desc, power)));
    }

    if (!result.empty())
        result.front()->run();
    TestHost::getInstance().setCurrentTime(now);

    return MinStretchScheduler::sortMinStretch(result, apps);
}


template<> shared_ptr<StretchInformation> AggregationTest<StretchInformation>::createInfo(const AggregationTest::Node & n) {
    shared_ptr<StretchInformation> s(new StretchInformation);
    // list<Time> q = createRandomLAF(n.power);
    list<StretchInformation::AppDesc> apps;
    createRandomQueue(n.mem, n.disk, n.power, apps);
    s->setAvailability(n.mem, n.disk, apps, n.power);
    totalInfo->join(*s);
// const StretchInformation::HSWFunction & minH = s->getSummary()[0].meanH;
// if (privateData.minAvail.getMinStretch() == 0.0)
//  privateData.minAvail = minH;
// else {
//  privateData.minAvail.min(privateData.minAvail, minH);
//  privateData.minAvail.reduce();
// }
// const TimeConstraintInfo::ATFunction * f[] = { &privateData.totalAvail, &minA };
// double c[] = { 1.0, 1.0 };
// privateData.totalAvail.lc(f, c);
    return s;
}


static string plot(const StretchInformation::HSWFunction & f) {
    double sh, wh;
    f.getHorizon(sh, wh);
    if (wh == 0) wh = 1000;
    std::ostringstream oss3d, oss2d;
    oss3d << "set width 20; set multiplot; set view -30,30; set samples 1000 grid 200x200; set lw 0.1; set key below; set xrange [" << f.getMinStretch() << ':' << (sh * 1.5) << "]; set yrange [0:" << (wh * 1.5) << ']' << std::endl;
    oss3d << "plot 3d ";
    oss2d << "plot ";
    const std::vector<StretchInformation::HSWFunction::Piece> & pieces = f.getPieces();
    for (unsigned int j = 0; j < pieces.size(); ++j) {
        const StretchInformation::HSWFunction::Piece & p = pieces[j];
        if (j > 0) {
            oss3d << ", ";
            oss2d << ", ";
        }
        if (p.f.a != 0.0) {
            if (p.f.b != 0.0) {
                if (p.f.c != 0.0) {
                    oss3d << "x*(y*" << p.f.a << '+' << p.f.b << ")-" << p.f.c;
                } else {
                    oss3d << "x*(y*" << p.f.a << '+' << p.f.b << ')';
                }
            } else {
                if (p.f.c != 0.0) {
                    oss3d << "x*y*" << p.f.a << '-' << p.f.c;
                } else {
                    oss3d << "x*y*" << p.f.a;
                }
            }
        } else {
            if (p.f.b != 0.0) {
                if (p.f.c != 0.0) {
                    oss3d << "x*" << p.f.b << '-' << p.f.c;
                } else {
                    oss3d << "x*" << p.f.b;
                }
            } else {
                oss3d << p.f.c;
            }
        }
        oss3d << " s $2>=";
        if (p.d != 0.0) {
            if (p.e != 0.0) {
                oss3d << p.d << "/$1+" << p.e;
                oss2d << p.d << "/x+" << p.e;
            } else {
                oss3d << p.d << "/$1";
                oss2d << p.d << "/x";
            }
        } else {
            oss3d << p.e;
            oss2d << p.e;
        }
        if (p.nw != -1) {
            const StretchInformation::HSWFunction::Piece & nw = pieces[p.nw];
            oss3d << " and $2<=";
            if (nw.d != 0.0) {
                if (nw.e != 0.0) {
                    oss3d << nw.d << "/$1+" << nw.e;
                    oss2d << ':' << nw.d << "/x+" << nw.e;
                } else {
                    oss3d << nw.d << "/$1";
                    oss2d << ':' << nw.d << "/x";
                }
            } else {
                oss3d << nw.e;
                oss2d << ':' << nw.e;
            }
        } else {
            oss2d << ':' << (wh * 1.5);
        }
        oss3d << " and $1>=" << p.s;
        oss2d << " s $1>=" << p.s;
        if (p.ns != -1) {
            const StretchInformation::HSWFunction::Piece & ns = pieces[p.ns];
            oss3d << " and $1<" << ns.s;
            oss2d << " and $1<" << ns.s;
        }
        oss3d << " title \"";
        p.output(pieces, oss3d);
        oss3d << "\" w su c " << j;
        oss2d << " title \"";
        p.output(pieces, oss2d);
        oss2d << "\" w yerrorsh col " << j << " fi " << j;
    }
    oss3d << std::endl << "set origin 20,-3" << std::endl << oss2d.str();
    return oss3d.str();
}


//static void checkOverlap(const StretchInformation::HSWFunction & f) {
// const std::vector<std::pair<StretchInformation::HSWFunction::Range, StretchInformation::HSWFunction::SubFunction> > & intervals = f.getIntervals();
// for (unsigned int j = 0; j < intervals.size(); ++j) {
//  // Check it does not overlap other ranges
//  for (unsigned int k = j + 1; k < intervals.size(); ++k) {
//   list<StretchInformation::HSWFunction::Range> in;
//   intervals[j].first.intersection(intervals[k].first, in);
//   ostringstream ossin;
//   for (list<StretchInformation::HSWFunction::Range>::iterator it = in.begin(); it != in.end(); ++it)
//    ossin << ' ' << *it;
//   BOOST_CHECK_MESSAGE(in.empty(), "Ranges " << j << ' ' << intervals[j].first << " and " << k << ' ' << intervals[k].first << " overlap!:" << ossin.str());
//  }
// }
//}


/// Test Cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(HSWFunction) {
    TestHost::getInstance().reset();

    // Min/max and sum of several functions
    ofstream of("hswf_test.ppl");
    for (int i = 0; i < 1000; i++) {
        LogMsg("Test.RI", INFO) << "Function " << i << ": ";
        double f11power = AggregationTest<StretchInformation>::uniform(1000, 3000, 200),
                          f12power = AggregationTest<StretchInformation>::uniform(1000, 3000, 200),
                                     f13power = AggregationTest<StretchInformation>::uniform(1000, 3000, 200),
                                                f21power = AggregationTest<StretchInformation>::uniform(1000, 3000, 200),
                                                           f22power = AggregationTest<StretchInformation>::uniform(1000, 3000, 200);
        list<StretchInformation::AppDesc> apps11, apps12, apps13, apps21, apps22;
        createRandomQueue(1024, 512, f11power, apps11);
        createRandomQueue(1024, 512, f12power, apps12);
        createRandomQueue(1024, 512, f13power, apps13);
        createRandomQueue(1024, 512, f21power, apps21);
        createRandomQueue(1024, 512, f22power, apps22);
        StretchInformation::HSWFunction
        f11(apps11, f11power),
        f12(apps12, f12power),
        f13(apps13, f13power),
        f21(apps21, f21power),
        f22(apps22, f22power);
//  checkOverlap(f11);
//  checkOverlap(f12);
//  checkOverlap(f13);
//  checkOverlap(f21);
//  checkOverlap(f22);
        double sh = 0.0, wh = 0.0;
        {
            double tmpsh, tmpwh;
            f11.getHorizon(tmpsh, tmpwh);
            if (tmpsh > sh) sh = tmpsh;
            if (tmpwh > wh) wh = tmpwh;
            f12.getHorizon(tmpsh, tmpwh);
            if (tmpsh > sh) sh = tmpsh;
            if (tmpwh > wh) wh = tmpwh;
            f13.getHorizon(tmpsh, tmpwh);
            if (tmpsh > sh) sh = tmpsh;
            if (tmpwh > wh) wh = tmpwh;
            f21.getHorizon(tmpsh, tmpwh);
            if (tmpsh > sh) sh = tmpsh;
            if (tmpwh > wh) wh = tmpwh;
            f22.getHorizon(tmpsh, tmpwh);
            if (tmpsh > sh) sh = tmpsh;
            if (tmpwh > wh) wh = tmpwh;
        }
        sh *= 1.2;
        wh *= 1.2;
//  StretchInformation::HSWFunction min, max;
//  min.min(f11, f12);
//  min.min(min, f13);
//  min.min(min, f21);
//  min.min(min, f22);
//  max.max(f11, f12);
//  max.max(max, f13);
//  max.max(max, f21);
//  max.max(max, f22);
//
//  // Join f11 with f12
//  StretchInformation::HSWFunction f112;
//  double accumAsq112 = f112.meanAndLoss(f11, f12, 1, 1, sh, wh);
//  checkOverlap(f112);
//  BOOST_CHECK_GE(accumAsq112, 0);
//  BOOST_CHECK_CLOSE(accumAsq112, f112.sqdiff(f11, sh, wh) + f112.sqdiff(f12, sh, wh), 0.0001);
//  BOOST_CHECK_CLOSE(2*accumAsq112, f11.sqdiff(f12, sh, wh), 0.0001);
//
//  // join f112 with f13, and that is f1
//  StretchInformation::HSWFunction f1;
//  double accumAsq1 = f1.meanAndLoss(f112, f13, 2, 1, sh, wh) + accumAsq112;
//  checkOverlap(f1);
//  BOOST_CHECK_GE(accumAsq1, 0);
//  BOOST_CHECK_CLOSE(accumAsq1, f1.sqdiff(f11, sh, wh) + f1.sqdiff(f12, sh, wh) + f1.sqdiff(f13, sh, wh), 0.0001);
//
//  // join f21 with f22, and that is f2
//  StretchInformation::HSWFunction f2;
//  double accumAsq2 = f2.meanAndLoss(f21, f22, 1, 1, sh, wh);
//  checkOverlap(f2);
//  BOOST_CHECK_GE(accumAsq2, 0);
//  BOOST_CHECK_CLOSE(accumAsq2, f2.sqdiff(f21, sh, wh) + f2.sqdiff(f22, sh, wh), 0.0001);
//
//  // join f1 with f2, and that is f
//  StretchInformation::HSWFunction f;
//  double accumAsq = f.meanAndLoss(f1, f2, 3, 2, sh, wh) + accumAsq1 + accumAsq2;
//  checkOverlap(f);
//  BOOST_CHECK_GE(accumAsq, 0);
//  BOOST_CHECK_CLOSE(accumAsq, f.sqdiff(f11, sh, wh) + f.sqdiff(f12, sh, wh) + f.sqdiff(f13, sh, wh) + f.sqdiff(f21, sh, wh) + f.sqdiff(f22, sh, wh), 0.0001);
//
//  StretchInformation::HSWFunction fred(f);
//  double accumAsqRed = accumAsq + 5 * fred.reduce(sh, wh, 10);
//  checkOverlap(fred);
//  // Check that the reduced function has no interval wich is an extension of another one
//  //checkNoExtension(fred);
//  BOOST_CHECK_GE(accumAsqRed, 0);

        // Print functions
        of << "# Functions " << i << endl;
        of << "# F" << i << " f11: " << f11 << endl << plot(f11) << endl;
        of << "# F" << i << " f12: " << f12 << endl << plot(f12) << endl;
//  of << "# F" << i << " f112: " << f112 << endl << plot(f112) << endl
//    << "# accumAsq112 " << accumAsq112 << " =? " << (f112.sqdiff(f11, sh, wh) + f112.sqdiff(f12, sh, wh)) << endl;
        of << "# F" << i << " f13: " << f13 << endl << plot(f13) << endl;
//  of << "# F" << i << " f1: " << f1 << endl << plot(f1) << endl
//    << "# accumAsq1 " << accumAsq1 << " =? " << (f1.sqdiff(f11, sh, wh) + f1.sqdiff(f12, sh, wh) + f1.sqdiff(f13, sh, wh)) << endl;
        of << "# F" << i << " f21: " << f21 << endl << plot(f21) << endl;
        of << "# F" << i << " f22: " << f22 << endl << plot(f22) << endl;
//  of << "# F" << i << " f2: " << f2 << endl << plot(f2) << endl
//    << "# accumAsq2 " << accumAsq2 << " =? " << (f2.sqdiff(f21, sh, wh) + f2.sqdiff(f22, sh, wh)) << endl;
//  of << "# F" << i << " f: " << f << endl << plot(f) << endl
//    << "# accumAsq " << accumAsq << " =? " << (f.sqdiff(f11, sh, wh) + f.sqdiff(f12, sh, wh) + f.sqdiff(f13, sh, wh) + f.sqdiff(f21, sh, wh) + f.sqdiff(f22, sh, wh)) << endl;
//  of << "# F" << i << " fred: " << fred << endl << plot(fred) << endl
//    << "# accumAsqRed " << accumAsqRed << " =? " << (fred.sqdiff(f11, sh, wh) + fred.sqdiff(f12, sh, wh) + fred.sqdiff(f13, sh, wh) + fred.sqdiff(f21, sh, wh) + fred.sqdiff(f22, sh, wh)) << endl;
        of << endl;
    }
    of.close();
}


/// StretchInformation
BOOST_AUTO_TEST_CASE(siMsg) {
    TestHost::getInstance().reset();

    // Ctor
    StretchInformation s1;

    // setMinimumStretch
    s1.setMinAndMaxStretch(0.5, 1.2);

    // getMinimumStretch
    BOOST_CHECK_EQUAL(s1.getMinimumStretch(), 0.5);

    // getMaximumStretch
    BOOST_CHECK_EQUAL(s1.getMaximumStretch(), 1.2);

    // TODO: update

    // TODO: Check other things
    list<StretchInformation::AppDesc> apps;
    createRandomQueue(1024, 512, 1000.0, apps);
    s1.setAvailability(1024, 512, apps, 1000.0);
    LogMsg("Test.RI", INFO) << s1;

    shared_ptr<StretchInformation> p;
    CheckMsgMethod::check(s1, p);
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor



//static string plot(StretchInformation::HSWFunction & f) {
// ostringstream os;
// if (f.getPoints().empty()) {
//  os << "0,0" << endl;
//  os << "100000000000," << (unsigned long)(f.getSlope() * 100000.0) << endl;
// } else {
//  for (vector<pair<Time, uint64_t> >::const_iterator it = f.getPoints().begin(); it != f.getPoints().end(); it++)
//   os << it->first.getRawDate() << ',' << it->second << endl;
// }
// return os.str();
//}


BOOST_AUTO_TEST_SUITE(Per)   // Performance test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(siAggr) {
    //Time ct = reference;
    ClusteringVector<StretchInformation::MDHCluster>::setDistVectorSize(20);
    unsigned int numpoints = 10;
    StretchInformation::setNumPieces(numpoints);
// ofstream off("asi_test_function.stat");
    ofstream ofmd("asi_test_mem_disk.stat");
    AggregationTest<StretchInformation> t;
    for (int i = 0; i < 2; i++) {
        for (int nc = 2; nc < 7; nc++) {
            StretchInformation::setNumClusters(nc * nc * nc);
//   off << "# " << (nc * nc * nc) << " clusters" << endl;
            ofmd << "# " << (nc * nc * nc) << " clusters" << endl;
            shared_ptr<StretchInformation> result = t.test(i);

            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
//   StretchInformation::HSWFunction minAvail, aggrAvail, treeAvail, dummy;
//   StretchInformation::ATFunction & totalAvail = const_cast<StretchInformation::ATFunction &>(t.getPrivateData().totalAvail);
//   const StretchInformation::ATFunction * f[2] = { &t.getPrivateData().minAvail, &dummy };
//   double c[2] = { t.getNumNodes(), 1.0 };
//   minAvail.lc(f, c);

            unsigned long int aggrMem = 0, aggrDisk = 0;
            {
//    f[0] = &aggrAvail; c[0] = 1.0;
                shared_ptr<StretchInformation> totalInformation = t.getTotalInformation();
                const ClusteringVector<StretchInformation::MDHCluster> & clusters = totalInformation->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const StretchInformation::MDHCluster & u = clusters[j];
                    aggrMem += (unsigned long int)u.minM * u.value;
                    aggrDisk += (unsigned long int)u.minD * u.value;
//     f[1] = &u.minA; c[1] = u.value;
//     aggrAvail.lc(f, c);
                }
            }

            unsigned long int treeMem = 0, treeDisk = 0;
            {
//    f[0] = &treeAvail; c[0] = 1.0;
                const ClusteringVector<StretchInformation::MDHCluster> & clusters = result->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const StretchInformation::MDHCluster & u = clusters[j];
//     BOOST_CHECK(u.minA.getPoints().size() <= numpoints);
//     BOOST_CHECK(u.accumMaxA.getPoints().size() <= numpoints);
                    treeMem += (unsigned long int)u.minM * u.value;
                    treeDisk += (unsigned long int)u.minD * u.value;
//     f[1] = &u.minA; c[1] = u.value;
//     treeAvail.lc(f, c);
                }
            }

            LogMsg("Test.RI", INFO) << "H. " << i << " nc. " << nc << ": min/mean/max " << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()
            << " mem " << (treeMem - minMem) << '/' << (t.getTotalMem() - minMem) << '(' << ((treeMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "%)"
            << " disk " << (treeDisk - minDisk) << '/' << (t.getTotalDisk() - minDisk) << '(' << ((treeDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "%)";
            //<< " avail " << deltaAggrAvail << '/' << deltaTotalAvail;
            LogMsg("Test.RI", INFO) << "N. " << t.getNumNodes() << " nc. " << nc
            << " mem " << (aggrMem - minMem) << '/' << (t.getTotalMem() - minMem) << '(' << ((aggrMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "%)"
            << " disk " << (aggrDisk - minDisk) << '/' << (t.getTotalDisk() - minDisk) << '(' << ((aggrDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "%)";
            //<< " avail " << deltaAggrAvail << '/' << deltaTotalAvail;
//   list<Time> p;
//   for (vector<pair<Time, uint64_t> >::const_iterator it = aggrAvail.getPoints().begin(); it != aggrAvail.getPoints().end(); it++)
//    p.push_back(it->first);
//   for (vector<pair<Time, uint64_t> >::const_iterator it = treeAvail.getPoints().begin(); it != treeAvail.getPoints().end(); it++)
//    p.push_back(it->first);
//   for (vector<pair<Time, uint64_t> >::const_iterator it = totalAvail.getPoints().begin(); it != totalAvail.getPoints().end(); it++)
//    p.push_back(it->first);
//   for (vector<pair<Time, uint64_t> >::const_iterator it = minAvail.getPoints().begin(); it != minAvail.getPoints().end(); it++)
//    p.push_back(it->first);
//   p.sort();
//   off << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << nc << ',' << t.getTotalMem() << ',' << minMem << ',' << (minMem * 100.0 / t.getTotalMem()) << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << ',' << treeMem << ',' << (treeMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << nc << ',' << t.getTotalDisk() << ',' << minDisk << ',' << (minDisk * 100.0 / t.getTotalDisk()) << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << ',' << treeDisk << ',' << (treeDisk * 100.0 / t.getTotalDisk()) << endl;
//   double lastTime = -1.0;
//   for (list<Time>::iterator it = p.begin(); it != p.end(); it++) {
//    unsigned long t = totalAvail.getAvailabilityBefore(*it),
//      a = aggrAvail.getAvailabilityBefore(*it),
//      at = treeAvail.getAvailabilityBefore(*it),
//      min = minAvail.getAvailabilityBefore(*it);
//    double time = floor((*it - ct).seconds() * 1000.0) / 1000.0;
//    if (lastTime != time) {
//     off << time << ',' << t
//       << ',' << min << ',' << (t == 0 ? 100 : min * 100.0 / t)
//       << ',' << a << ',' << (t == 0 ? 100 : a * 100.0 / t)
//       << ',' << at << ',' << (t == 0 ? 100 : at * 100.0 / t) << endl;
//     lastTime = time;
//    }
//   }
//   off << endl;
            ofmd << endl;
        }
//  off << endl;
        ofmd << endl;
    }
// off.close();
    ofmd.close();
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Per
