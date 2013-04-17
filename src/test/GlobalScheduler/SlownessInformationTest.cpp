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
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "CheckMsg.hpp"
#include "TestHost.hpp"
#include "../ExecutionManager/TestTask.hpp"
#include "MSPAvailabilityInformation.hpp"
#include "MSPScheduler.hpp"
using namespace std;
using namespace boost::random;


namespace {

bool orderById(const TaskProxy & l, const TaskProxy & r) {
    return l.id < r.id;
}


void getProxys(const list<shared_ptr<Task> > & tasks, list<TaskProxy> & result, vector<double> & lBounds) {
    if (!tasks.empty()) {
        for (list<shared_ptr<Task> >::const_iterator i = tasks.begin(); i != tasks.end(); ++i)
            result.push_back(TaskProxy(*i));
        TaskProxy::getSwitchValues(result, lBounds);
    }
}


double createRandomQueue(mt19937 & gen, double power, list<TaskProxy> & proxys, vector<double> & lBounds) {
    static unsigned int id = 0;

    Time now = TestHost::getInstance().getCurrentTime();
    proxys.clear();

    // Add a random number of applications, with random length and number of tasks
    for(int appid = 0; uniform_int_distribution<>(1, 3)(gen) != 1; ++appid) {
        double r = uniform_int_distribution<>(-1000, 0)(gen);
        unsigned int numTasks = uniform_int_distribution<>(1, 10)(gen);
        // Applications between 1-4h on a 1000 MIPS computer
        int a = uniform_int_distribution<>(600000, 14400000)(gen) / numTasks;
        for (unsigned int taskid = 0; taskid < numTasks; ++taskid) {
            proxys.push_back(TaskProxy(a, power, now + Duration(r)));
            proxys.back().id = id++;
        }
    }

    if (!proxys.empty()) {
        TaskProxy::getSwitchValues(proxys, lBounds);
        TaskProxy::sortMinSlowness(proxys, lBounds);
        return TaskProxy::getSlowness(proxys);
    } else return 0.0;
}


double createNLengthQueue(mt19937 & gen, double power, list<TaskProxy> & proxys, vector<double> & lBounds, int n) {
    static unsigned int id = 0;
    Time now = TestHost::getInstance().getCurrentTime();
    proxys.clear();

    // Add n tasks with random length
    for(int appid = 0; appid < n; ++appid) {
        double r = uniform_int_distribution<>(-1000, 0)(gen);
        // Applications between 1-4h on a 1000 MIPS computer
        int a = uniform_int_distribution<>(600000, 14400000)(gen);
        proxys.push_back(TaskProxy(a, power, now + Duration(r)));
        proxys.back().id = id++;
    }

    if (!proxys.empty()) {
        TaskProxy::getSwitchValues(proxys, lBounds);
        TaskProxy::sortMinSlowness(proxys, lBounds);
        return TaskProxy::getSlowness(proxys);
    } else return 0.0;
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
        if (!proxys.empty()) {
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
                proxys.push_back(TaskProxy(a, power, now));
            TaskProxy::sortMinSlowness(proxys, lBoundsTmp);
        } else
            for (int i = 0; i < n; ++i)
                proxys.push_back(TaskProxy(a, power, now));
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

double maxDifference(list<TaskProxy> proxys, const vector<double> & lBounds, double power, double ah, const MSPAvailabilityInformation::LAFunction & f) {
    uint64_t astep = (ah - MSPAvailabilityInformation::LAFunction::minTaskLength) / 100;
    Time now = Time::getCurrentTime();
    double maxDiff = 1.0;
    for (uint64_t a = MSPAvailabilityInformation::LAFunction::minTaskLength; a < ah; a += astep) {
        // Add a new task of length a
        if (!proxys.empty()) {
            vector<double> lBoundsTmp(lBounds);
            for (list<TaskProxy>::iterator it = ++proxys.begin(); it != proxys.end(); ++it)
                if (it->a != a) {
                    double l = (now - it->rabs).seconds() / (it->a - a);
                    if (l > 0.0) {
                        lBoundsTmp.push_back(l);
                    }
                }
            std::sort(lBoundsTmp.begin(), lBoundsTmp.end());
            proxys.push_back(TaskProxy(a, power, now));
            TaskProxy::sortMinSlowness(proxys, lBoundsTmp);
        } else
            proxys.push_back(TaskProxy(a, power, now));
        double estimate = f.estimateSlowness(a, 1), real = 0.0;
        Time e = Time::getCurrentTime();
        // For each task, calculate finishing time
        for (list<TaskProxy>::iterator i = proxys.begin(); i != proxys.end(); ++i) {
            e += Duration(i->t);
            double slowness = (e - i->rabs).seconds() / i->a;
            if (slowness > real)
                real = slowness;
        }
        double difference = real / estimate;
        if (difference > maxDiff) maxDiff = difference;
        for (list<TaskProxy>::iterator i = proxys.begin(); i != proxys.end();) {
            if (i->id == (unsigned int)-1)
                proxys.erase(i++);
            else ++i;
        }
    }
    return maxDiff;
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


/// Test Cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)


BOOST_AUTO_TEST_CASE(LAFunction) {
    TestHost::getInstance().reset();
    mt19937 gen;

    // Min/max and sum of several functions
    ofstream of("laf_test.ppl");
    ofstream ofs("laf_test.stat");
    MSPAvailabilityInformation::setNumPieces(3);
    for (int i = 0; i < 100; i++) {
        LogMsg("Test.RI", INFO) << "Function " << i << ": ";
        double f11power = floor(uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200,
                f12power = floor(uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200,
                f13power = floor(uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200,
                f21power = floor(uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200,
                f22power = floor(uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200;
        list<TaskProxy> proxys11, proxys12, proxys13, proxys21, proxys22;
        vector<double> lBounds11, lBounds12, lBounds13, lBounds21, lBounds22;
        createRandomQueue(gen, f11power, proxys11, lBounds11);
        createRandomQueue(gen, f12power, proxys12, lBounds12);
        createRandomQueue(gen, f13power, proxys13, lBounds13);
        createRandomQueue(gen, f21power, proxys21, lBounds21);
        createRandomQueue(gen, f22power, proxys22, lBounds22);
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

        Time now = Time::getCurrentTime();
        if (!proxys11.empty()) {
            double d = proxys11.front().t;
            proxys11.front().t = 0;
            TestHost::getInstance().setCurrentTime(now + Duration(d));
            BOOST_CHECK_LE(maxDifference(proxys11, lBounds11, f11power, ah, f11), 1.01);
            proxys11.front().t = d;
        }
        TestHost::getInstance().setCurrentTime(now);

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
        ofs << "# Estimation with 1 task" << endl;
        plotSampled(proxys11, lBounds11, f11power, f11.getHorizon()*1.2, 1, f11, ofs);
        ofs << endl;
        if (!proxys11.empty()) {
            double d = proxys11.front().t;
            proxys11.front().t = 0;
            TestHost::getInstance().setCurrentTime(now + Duration(d));
            ofs << "# Estimation with 1 task at the end of first task" << endl;
            plotSampled(proxys11, lBounds11, f11power, f11.getHorizon()*1.2, 1, f11, ofs);
            ofs << endl;
            proxys11.front().t = d;
        }
        TestHost::getInstance().setCurrentTime(now);
        for (int n = 2; n < 6; ++n) {
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
    mt19937 gen;

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
    createRandomQueue(gen, 1000.0, proxys, lBounds);
    s1.setAvailability(1024, 512, proxys, lBounds, 1000.0, 0.5);
    LogMsg("Test.RI", INFO) << s1;

    shared_ptr<MSPAvailabilityInformation> p;
    CheckMsgMethod::check(s1, p);
}


/// SlownessAlgorithm
BOOST_AUTO_TEST_CASE(mspAlg) {
    TestHost::getInstance().reset();
    mt19937 gen;

    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            list<TaskProxy> proxys;
            vector<double> lBounds;
            double slowness = createNLengthQueue(gen, floor(uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200, proxys, lBounds, i);
            ostringstream oss;
            for (list<TaskProxy>::iterator it = proxys.begin(); it != proxys.end(); ++it) {
                oss << *it;
            }
            // Check that no other sorting produces a lower slowness.
            proxys.sort(orderById);
            do {
                double s = TaskProxy::getSlowness(proxys);
                BOOST_CHECK_LE(slowness, s);
                if (slowness > s) {
                    cout << "Sorted queue: " << oss.str() << endl;
                    cout << "Current queue: ";
                    for (list<TaskProxy>::iterator it = proxys.begin(); it != proxys.end(); ++it) {
                        cout << *it;
                    }
                    cout << endl;
                }
            } while (std::next_permutation(++proxys.begin(), proxys.end(), orderById));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor
