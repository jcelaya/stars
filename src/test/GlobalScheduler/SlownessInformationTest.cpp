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

#include <sstream>
#include <boost/test/unit_test.hpp>
#include "CheckMsg.hpp"
#include "TestHost.hpp"
#include "AggregationTest.hpp"
#include "../ExecutionManager/TestTask.hpp"
#include "MSPAvailabilityInformation.hpp"
#include "MSPScheduler.hpp"
using std::list;


template<> struct Priv<MSPAvailabilityInformation> {
    MSPAvailabilityInformation::LAFunction totalAvail;
    MSPAvailabilityInformation::LAFunction maxAvail;
};


namespace {
MSPAvailabilityInformation::LAFunction dummy;


void getProxys(const list<shared_ptr<Task> > & tasks, list<TaskProxy> & result, vector<double> & lBounds) {
    for (list<shared_ptr<Task> >::const_iterator i = tasks.begin(); i != tasks.end(); ++i)
        result.push_back(TaskProxy(*i));
    for (list<TaskProxy>::iterator i = ++result.begin(); i != result.end(); ++i)
        for (list<TaskProxy>::iterator j = i; j != result.end(); ++j)
            if (i->a != j->a) {
                double l = (j->rabs - i->rabs).seconds() / (i->a - j->a);
                if (l > 0.0) {
                    lBounds.push_back(l);
                }
            }
    std::sort(lBounds.begin(), lBounds.end());
    vector<double>::iterator last = std::unique(lBounds.begin(), lBounds.end());
    lBounds.resize(last - lBounds.begin());
}


double createRandomQueue(unsigned int maxmem, unsigned int maxdisk, double power, list<TaskProxy> & proxys, vector<double> & lBounds) {
    Time now = TestHost::getInstance().getCurrentTime();
    list<shared_ptr<Task> > tasks;

    // Add a random number of applications, with random length and number of tasks
    for(int appid = 0; AggregationTest<MSPAvailabilityInformation>::uniform(1, 3) != 1; ++appid) {
        double r = AggregationTest<MSPAvailabilityInformation>::uniform(-1000, 0);
        TaskDescription desc;
        // Applications between 1-4h on a 1000 MIPS computer
        int w = AggregationTest<MSPAvailabilityInformation>::uniform(600000, 14400000);
        desc.setNumTasks(AggregationTest<MSPAvailabilityInformation>::uniform(1, 10));
        //desc.setNumTasks(1);
        desc.setLength(w / desc.getNumTasks());
        desc.setMaxMemory(AggregationTest<MSPAvailabilityInformation>::uniform(1, maxmem));
        desc.setMaxDisk(AggregationTest<MSPAvailabilityInformation>::uniform(1, maxdisk));
        TestHost::getInstance().setCurrentTime(now + Duration(r));
        for (unsigned int taskid = 0; taskid < desc.getNumTasks(); ++taskid)
            tasks.push_back(shared_ptr<Task>(new TestTask(CommAddress(), appid, taskid, desc, power)));
    }

    if (!tasks.empty())
        tasks.front()->run();
    TestHost::getInstance().setCurrentTime(now);

    getProxys(tasks, proxys, lBounds);

    return MSPScheduler::sortMinSlowness(proxys, lBounds, tasks);
}


double createNLengthQueue(unsigned int maxmem, unsigned int maxdisk, double power, list<TaskProxy> & proxys, vector<double> & lBounds, int n) {
    Time now = TestHost::getInstance().getCurrentTime();
    list<shared_ptr<Task> > tasks;

    // Add n tasks with random length
    for(int appid = 0; appid < n; ++appid) {
        double r = AggregationTest<MSPAvailabilityInformation>::uniform(-1000, 0);
        TaskDescription desc;
        // Applications between 1-4h on a 1000 MIPS computer
        int a = AggregationTest<MSPAvailabilityInformation>::uniform(600000, 14400000);
        desc.setNumTasks(1);
        desc.setLength(a);
        desc.setMaxMemory(maxmem);
        desc.setMaxDisk(maxdisk);
        TestHost::getInstance().setCurrentTime(now + Duration(r));
        tasks.push_back(shared_ptr<Task>(new TestTask(CommAddress(), appid, 0, desc, power)));
    }

    TestHost::getInstance().setCurrentTime(now);
    if (!tasks.empty())
        tasks.front()->run();

    getProxys(tasks, proxys, lBounds);

    return MSPScheduler::sortMinSlowness(proxys, lBounds, tasks);
}


string plot(const MSPAvailabilityInformation::LAFunction & f, double ah) {
    std::ostringstream oss;
    oss << "plot [" << MSPAvailabilityInformation::LAFunction::minTaskLength << ':' << ah << "] ";
    const std::vector<std::pair<double, MSPAvailabilityInformation::LAFunction::SubFunction> > & pieces = f.getPieces();
    for (unsigned int j = 0; j < pieces.size(); ++j) {
        const MSPAvailabilityInformation::LAFunction::SubFunction & p = pieces[j].second;
        if (j > 0) {
            oss << ", ";
        }
        oss << p.x << "/x + " << p.y << "*x + " << p.z1 << " + " << p.z2 << " s $1 >= " << pieces[j].first;
        if (j < pieces.size() - 1)
                oss << " and $1 < " << pieces[j + 1].first;
        oss << " title \"" << p << "\" w lines col " << j;
    }
    return oss.str();
}

void plotSampled(list<TaskProxy> proxys, const vector<double> & lBounds, double power, double ah, int n, const MSPAvailabilityInformation::LAFunction & f, std::ostream & os) {
    uint64_t astep = (ah - MSPAvailabilityInformation::LAFunction::minTaskLength) / 100;
    Time now = Time::getCurrentTime();
    for (uint64_t a = MSPAvailabilityInformation::LAFunction::minTaskLength; a < ah; a += astep) {
        // Add a new task of length a
        TaskDescription desc;
        desc.setLength(a);
        vector<double> lBoundsTmp(lBounds);
        for (list<TaskProxy>::iterator it = ++proxys.begin(); it != proxys.end(); ++it)
            if (it->a != a) {
                double l = (now - it->rabs).seconds() / (it->a - a);
                if (l > 0.0) {
                    lBoundsTmp.push_back(l);
                }
            }
        std::sort(lBoundsTmp.begin(), lBoundsTmp.end());
        for (int i = 0; i < n; ++i)
            proxys.push_back(TaskProxy(a, power));
        TaskProxy::sortMinSlowness(proxys, lBoundsTmp);
        double estimate = f.estimateSlowness(a, n), real = 0.0;
        Time e = Time::getCurrentTime();
        // For each task, calculate finishing time
        for (list<TaskProxy>::iterator i = proxys.begin(); i != proxys.end(); ++i) {
            e += Duration(i->t);
            double slowness = (e - i->rabs).seconds() / i->a;
            if (slowness > real)
                real = slowness;
        }
        double difference;
        {
            double diff = std::fabs(estimate - real);
            double d1 = diff / std::fabs(real);
            double d2 = diff / std::fabs(estimate);
            difference = d1 < d2 ? d2 : d1;
        }
        os << a << ',' << estimate << ',' << real << ',' << difference << "  # ";
        for (list<TaskProxy>::iterator i = proxys.begin(); i != proxys.end();) {
            os << i->id << ',';
            if (i->id == (unsigned int)-1)
                proxys.erase(i++);
            else ++i;
        }
        os << endl;
    }
}

bool isMax(const MSPAvailabilityInformation::LAFunction & f1,
        const MSPAvailabilityInformation::LAFunction & f2,
        const MSPAvailabilityInformation::LAFunction & max,
        uint64_t ah, uint64_t astep) {
    for (uint64_t a = MSPAvailabilityInformation::LAFunction::minTaskLength; a < ah; a += astep) {
        double l1 = f1.getSlowness(a), l2 = f2.getSlowness(a), lmax = max.getSlowness(a);
        if ((l1 > l2 && lmax != l1) || (l1 <= l2 && lmax != l2))
            return false;
    }
    return true;
}
}


template<> shared_ptr<MSPAvailabilityInformation> AggregationTest<MSPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    shared_ptr<MSPAvailabilityInformation> s(new MSPAvailabilityInformation);
    list<TaskProxy> proxys;
    vector<double> lBounds;
    double minSlowness = createRandomQueue(n.mem, n.disk, n.power, proxys, lBounds);
    s->setAvailability(n.mem, n.disk, proxys, lBounds, n.power, minSlowness);
    totalInfo->join(*s);
    const MSPAvailabilityInformation::LAFunction & maxL = s->getSummary()[0].maxL;
    if (privateData.maxAvail == MSPAvailabilityInformation::LAFunction()) {
        privateData.maxAvail = maxL;
    } else {
        privateData.maxAvail.max(privateData.maxAvail, maxL);
        //privateData.minAvail.reduce();
    }
    privateData.totalAvail.maxDiff(dummy, dummy, 1, 1, maxL, privateData.totalAvail);
    return s;
}


/// Test Cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(LAFunction) {
    TestHost::getInstance().reset();

    // Min/max and sum of several functions
    ofstream of("laf_test.ppl");
    ofstream ofs("laf_test.stat");
    MSPAvailabilityInformation::setNumPieces(3);
    for (int i = 0; i < 100; i++) {
        LogMsg("Test.RI", INFO) << "Function " << i << ": ";
        double f11power = AggregationTest<MSPAvailabilityInformation>::uniform(1000, 3000, 200),
                f12power = AggregationTest<MSPAvailabilityInformation>::uniform(1000, 3000, 200),
                f13power = AggregationTest<MSPAvailabilityInformation>::uniform(1000, 3000, 200),
                f21power = AggregationTest<MSPAvailabilityInformation>::uniform(1000, 3000, 200),
                f22power = AggregationTest<MSPAvailabilityInformation>::uniform(1000, 3000, 200);
        list<TaskProxy> proxys11, proxys12, proxys13, proxys21, proxys22;
        vector<double> lBounds11, lBounds12, lBounds13, lBounds21, lBounds22;
        createRandomQueue(1024, 512, f11power, proxys11, lBounds11);
        createRandomQueue(1024, 512, f12power, proxys12, lBounds12);
        createRandomQueue(1024, 512, f13power, proxys13, lBounds13);
        createRandomQueue(1024, 512, f21power, proxys21, lBounds21);
        createRandomQueue(1024, 512, f22power, proxys22, lBounds22);
        MSPAvailabilityInformation::LAFunction
                f11(proxys11, lBounds11, f11power),
                f12(proxys12, lBounds12, f12power),
                f13(proxys13, lBounds13, f13power),
                f21(proxys21, lBounds21, f21power),
                f22(proxys22, lBounds22, f22power);
        double ah = 0.0;
        {
            double tmpah;
            tmpah = f11.getHorizon();
            if (tmpah > ah) ah = tmpah;
            tmpah = f12.getHorizon();
            if (tmpah > ah) ah = tmpah;
            tmpah = f13.getHorizon();
            if (tmpah > ah) ah = tmpah;
            tmpah = f21.getHorizon();
            if (tmpah > ah) ah = tmpah;
            tmpah = f22.getHorizon();
            if (tmpah > ah) ah = tmpah;
        }
        ah *= 1.2;
        uint64_t astep = (ah - MSPAvailabilityInformation::LAFunction::minTaskLength) / 100;

        MSPAvailabilityInformation::LAFunction min, max;
        min.min(f11, f12);
        min.min(min, f13);
        min.min(min, f21);
        min.min(min, f22);
        max.max(f11, f12);
        BOOST_CHECK_PREDICATE(isMax, (f11)(f12)(max)(ah)(astep));
        max.max(max, f13);
        BOOST_CHECK_PREDICATE(isMax, (f13)(max)(max)(ah)(astep));
        max.max(max, f21);
        BOOST_CHECK_PREDICATE(isMax, (f21)(max)(max)(ah)(astep));
        max.max(max, f22);
        BOOST_CHECK_PREDICATE(isMax, (f22)(max)(max)(ah)(astep));

        // Check one of the functions
        for (uint64_t a = MSPAvailabilityInformation::LAFunction::minTaskLength; a < ah;
                a += (ah - MSPAvailabilityInformation::LAFunction::minTaskLength) / 1000) {
            // Check the estimation of one task
            BOOST_CHECK_CLOSE(f11.getSlowness(a), f11.estimateSlowness(a, 1), 0.01);
        }

        // Join f11 with f12
        MSPAvailabilityInformation::LAFunction f112;
        double accumAsq112 = f112.maxAndLoss(f11, f12, 1, 1, MSPAvailabilityInformation::LAFunction(), MSPAvailabilityInformation::LAFunction(), ah);
        MSPAvailabilityInformation::LAFunction accumAln112;
        //accumAln112.max(f11, f12);
        accumAln112.maxDiff(f11, f12, 1, 1, MSPAvailabilityInformation::LAFunction(), MSPAvailabilityInformation::LAFunction());
        BOOST_CHECK_PREDICATE(isMax, (f11)(f12)(f112)(ah)(astep));
        BOOST_CHECK_GE(accumAsq112, 0);
        BOOST_CHECK_CLOSE(accumAsq112, f112.sqdiff(f11, ah) + f112.sqdiff(f12, ah), 0.0001);
        BOOST_CHECK_CLOSE(accumAsq112, f11.sqdiff(f12, ah), 0.0001);

        // join f112 with f13, and that is f1
        MSPAvailabilityInformation::LAFunction f1;
        double accumAsq1 = f1.maxAndLoss(f112, f13, 2, 1, accumAln112, MSPAvailabilityInformation::LAFunction(), ah) + accumAsq112;
        MSPAvailabilityInformation::LAFunction accumAln1;
        accumAln1.maxDiff(f112, f13, 2, 1, accumAln112, MSPAvailabilityInformation::LAFunction());
        BOOST_CHECK_PREDICATE(isMax, (f112)(f13)(f1)(ah)(astep));
        BOOST_CHECK_GE(accumAsq1, 0);
        BOOST_CHECK_CLOSE(accumAsq1, f1.sqdiff(f11, ah) + f1.sqdiff(f12, ah) + f1.sqdiff(f13, ah), 0.0001);

        // join f21 with f22, and that is f2
        MSPAvailabilityInformation::LAFunction f2;
        double accumAsq2 = f2.maxAndLoss(f21, f22, 1, 1, MSPAvailabilityInformation::LAFunction(), MSPAvailabilityInformation::LAFunction(), ah);
        MSPAvailabilityInformation::LAFunction accumAln2;
        accumAln2.maxDiff(f21, f22, 1, 1, MSPAvailabilityInformation::LAFunction(), MSPAvailabilityInformation::LAFunction());
        BOOST_CHECK_PREDICATE(isMax, (f21)(f22)(f2)(ah)(astep));
        BOOST_CHECK_GE(accumAsq2, 0);
        BOOST_CHECK_CLOSE(accumAsq2, f2.sqdiff(f21, ah) + f2.sqdiff(f22, ah), 0.0001);

        // join f1 with f2, and that is f
        MSPAvailabilityInformation::LAFunction f;
        double accumAsq = f.maxAndLoss(f1, f2, 3, 2, accumAln1, accumAln2, ah) + accumAsq1 + accumAsq2;
        MSPAvailabilityInformation::LAFunction accumAln;
        accumAln.maxDiff(f1, f2, 3, 2, accumAln1, accumAln2);
        BOOST_CHECK_PREDICATE(isMax, (f1)(f2)(f)(ah)(astep));
        BOOST_CHECK_GE(accumAsq, 0);
        BOOST_CHECK_CLOSE(accumAsq, f.sqdiff(f11, ah) + f.sqdiff(f12, ah) + f.sqdiff(f13, ah) + f.sqdiff(f21, ah) + f.sqdiff(f22, ah), 0.0001);

        MSPAvailabilityInformation::LAFunction fred(f);
        double accumAsqRed = accumAsq + 5 * fred.reduceMax(4, ah);
        BOOST_CHECK_GE(accumAsqRed, 0);
        for (uint64_t a = MSPAvailabilityInformation::LAFunction::minTaskLength; a < ah; a += astep) {
            BOOST_CHECK_GE(fred.getSlowness(a), f.getSlowness(a));
            if (fred.getSlowness(a) < f.getSlowness(a))
                break;
        }

        // Print functions
        of << "# Functions " << i << endl;
        ofs << "# Functions " << i << endl;
        of << "# F" << i << " f11: " << f11 << endl << plot(f11, ah) << ", \"laf_test.stat\" i " << i << " e :::0::0 w lines" << endl;
        ofs << "# F" << i << " f11: " << f11 << endl;
        for (int n = 1; n < 6; ++n) {
            ofs << "# Estimation with " << n << " tasks" << endl;
            plotSampled(proxys11, lBounds11, f11power, f11.getHorizon()*1.2, n, f11, ofs);
            ofs << endl;
        }
        of << "# F" << i << " f12: " << f12 << endl << plot(f12, ah) << endl;
        of << "# F" << i << " f112: " << f112 << endl << plot(f112, ah) << endl
                << "# accumAsq112 " << accumAsq112 << " =? " << (f112.sqdiff(f11, ah) + f112.sqdiff(f12, ah)) << endl;
        of << "# F" << i << " f13: " << f13 << endl << plot(f13, ah) << endl;
        of << "# F" << i << " f1: " << f1 << endl << plot(f1, ah) << endl
                << "# accumAsq1 " << accumAsq1 << " =? " << (f1.sqdiff(f11, ah) + f1.sqdiff(f12, ah) + f1.sqdiff(f13, ah)) << endl;
        of << "# F" << i << " f21: " << f21 << endl << plot(f21, ah) << endl;
        of << "# F" << i << " f22: " << f22 << endl << plot(f22, ah) << endl;
        of << "# F" << i << " f2: " << f2 << endl << plot(f2, ah) << endl
                << "# accumAsq2 " << accumAsq2 << " =? " << (f2.sqdiff(f21, ah) + f2.sqdiff(f22, ah)) << endl;
        of << "# F" << i << " f: " << f << endl << plot(f, ah) << endl
                << "# accumAsq " << accumAsq << " =? " << (f.sqdiff(f11, ah) + f.sqdiff(f12, ah) + f.sqdiff(f13, ah) + f.sqdiff(f21, ah) + f.sqdiff(f22, ah)) << endl;
        of << "# F" << i << " fred: " << fred << endl << plot(fred, ah) << endl << "# accumAsqRed " << accumAsqRed << endl;
        of << "# F" << i << " min: " << min << endl << plot(min, ah) << endl;
        of << "# F" << i << " max: " << max << endl << plot(max, ah) << endl;
        of << endl;
        ofs << endl << endl;
    }
    of.close();
    ofs.close();
}


/// SlownessInformation
BOOST_AUTO_TEST_CASE(siMsg) {
    TestHost::getInstance().reset();

    // Ctor
    MSPAvailabilityInformation s1;

    // setMinimumStretch
    s1.setMinimumSlowness(0.5);

    // getMinimumStretch
    BOOST_CHECK_EQUAL(s1.getMinimumSlowness(), 0.5);

    // TODO: update

    // TODO: Check other things
    list<TaskProxy> proxys;
    vector<double> lBounds;
    createRandomQueue(1024, 512, 1000.0, proxys, lBounds);
    s1.setAvailability(1024, 512, proxys, lBounds, 1000.0, 0.5);
    LogMsg("Test.RI", INFO) << s1;

    shared_ptr<MSPAvailabilityInformation> p;
    CheckMsgMethod::check(s1, p);
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor



BOOST_AUTO_TEST_SUITE(Per)   // Performance test suite

BOOST_AUTO_TEST_SUITE(aiTS)

BOOST_AUTO_TEST_CASE(LAFunction) {
    // Test the cost of the creation of an LAFunction
    ofstream of("laf_performance.stat");
    of << "# Number of tasks, time" << endl;
    double power = 1000.0;
    for (int i = 0; i < 1000; ++i) {
        double time = 0.0;
        for (int j = 0; j < 10; ++j) {
            ptime start = microsec_clock::universal_time();
            list<TaskProxy> proxys;
            vector<double> lBounds;
            createNLengthQueue(1024, 512, power, proxys, lBounds, i);
            MSPAvailabilityInformation::LAFunction f(proxys, lBounds, power);
            time += (microsec_clock::universal_time() - start).total_microseconds();
        }
        time /= 10000000.0;
        of << i << ',' << time << endl;
        cout << i << " tasks need " << time << endl;
    }
}


class Field {
    ostringstream oss;
    unsigned int width, written;
public:
    Field(unsigned int w) : width(w), written(0) {}
    template<class T> Field & operator<<(const T & t) {
        oss << t;
        written = oss.tellp();
        return *this;
    }
    friend ostream & operator<<(ostream & os, const Field & f) {
        os << f.oss.str();
        if (f.written < f.width)
            os << string(f.width - f.written, ' ');
        return os;
    }
};


BOOST_AUTO_TEST_CASE(siAggr) {
    //Time ct = reference;
    ClusteringVector<MSPAvailabilityInformation::MDLCluster>::setDistVectorSize(20);
    unsigned int numpoints = 8;
    MSPAvailabilityInformation::setNumPieces(numpoints);
    ofstream off("asi_test_function.stat");
    ofstream ofmd("asi_test_mem_disk.stat");
    AggregationTest<MSPAvailabilityInformation> t;
    for (int i = 0; i < 10; i++) {
        for (int nc = 2; nc < 7; nc++) {
            MSPAvailabilityInformation::setNumClusters(nc * nc * nc);
            off << "# " << (nc * nc * nc) << " clusters" << endl;
            ofmd << "# " << (nc * nc * nc) << " clusters" << endl;
            shared_ptr<MSPAvailabilityInformation> result = t.test(i);

            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            MSPAvailabilityInformation::LAFunction maxAvail, aggrAvail, treeAvail;
            MSPAvailabilityInformation::LAFunction & totalAvail = const_cast<MSPAvailabilityInformation::LAFunction &>(t.getPrivateData().totalAvail);
            maxAvail.maxDiff(t.getPrivateData().maxAvail, dummy, t.getNumNodes(), t.getNumNodes(), dummy, dummy);

            unsigned long int aggrMem = 0, aggrDisk = 0;
            {
                shared_ptr<MSPAvailabilityInformation> totalInformation = t.getTotalInformation();
                const ClusteringVector<MSPAvailabilityInformation::MDLCluster> & clusters = totalInformation->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const MSPAvailabilityInformation::MDLCluster & u = clusters[j];
                    aggrMem += (unsigned long int)u.minM * u.value;
                    aggrDisk += (unsigned long int)u.minD * u.value;
                    aggrAvail.maxDiff(u.maxL, dummy, u.value, u.value, aggrAvail, dummy);
                }
            }

            unsigned long int treeMem = 0, treeDisk = 0;
            {
                const ClusteringVector<MSPAvailabilityInformation::MDLCluster> & clusters = result->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const MSPAvailabilityInformation::MDLCluster & u = clusters[j];
                    BOOST_CHECK(u.maxL.getPieces().size() <= numpoints);
                    BOOST_CHECK(u.accumMaxL.getPieces().size() <= numpoints);
                    treeMem += (unsigned long int)u.minM * u.value;
                    treeDisk += (unsigned long int)u.minD * u.value;
                    treeAvail.maxDiff(u.maxL, dummy, u.value, u.value, treeAvail, dummy);
                }
            }

            double meanTotalAvail = 0.0, meanAggrAvail = 0.0, meanTreeAvail = 0.0, meanMaxAvail = 0.0;

            off << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            double ah = totalAvail.getHorizon() * 1.2;
            uint64_t astep = (ah - MSPAvailabilityInformation::LAFunction::minTaskLength) / 100;
            for (uint64_t a = MSPAvailabilityInformation::LAFunction::minTaskLength; a < ah; a += astep) {
                double t = totalAvail.getSlowness(a),
                        aa = aggrAvail.getSlowness(a),
                        at = treeAvail.getSlowness(a),
                        max = maxAvail.getSlowness(a);
                meanTotalAvail += t / 100;
                meanAggrAvail += aa / 100;
                meanTreeAvail += at / 100;
                meanMaxAvail += max / 100;
                off << a << ',' << t
                        << ',' << max << ',' << (t == 0 ? 100 : max * 100.0 / t)
                        << ',' << aa << ',' << (t == 0 ? 100 : aa * 100.0 / t)
                        << ',' << at << ',' << (t == 0 ? 100 : at * 100.0 / t) << endl;
            }
            off << endl;

            LogMsg("Test.RI", INFO) << i << ' ' << setw(6) << t.getNumNodes() << ' ' << nc << ' '
                    << (Field(15) << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()) << "  "
                    << setiosflags(ios_base::fixed | ios_base::right) << setprecision(2)
                    << setw(7) << ((treeMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "% "
                    << setw(7) << ((treeDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "% "
                    << setw(7) << ((meanMaxAvail - meanTreeAvail) * 100.0 / (meanMaxAvail - meanTotalAvail)) << "%   "
                    << setw(7) << ((aggrMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "% "
                    << setw(7) << ((aggrDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "% "
                    << setw(7) << ((meanMaxAvail - meanAggrAvail) * 100.0 / (meanMaxAvail - meanTotalAvail)) << "%";

            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << nc << ',' << t.getTotalMem() << ',' << minMem << ',' << (minMem * 100.0 / t.getTotalMem()) << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << ',' << treeMem << ',' << (treeMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << nc << ',' << t.getTotalDisk() << ',' << minDisk << ',' << (minDisk * 100.0 / t.getTotalDisk()) << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << ',' << treeDisk << ',' << (treeDisk * 100.0 / t.getTotalDisk()) << endl;
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
