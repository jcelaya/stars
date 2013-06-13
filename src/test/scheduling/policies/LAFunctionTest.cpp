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
#include <fstream>
#include <boost/test/unit_test.hpp>
#include <utility>
#include "TestHost.hpp"
#include "LAFunction.hpp"
#include "RandomQueueGenerator.hpp"
using namespace std;


namespace stars {

template <typename Func, int resolution = 100>
void forAinDomain(uint64_t horizon, Func f) {
    uint64_t astep = (horizon - LAFunction::minTaskLength) / resolution;
    for (uint64_t a = LAFunction::minTaskLength; a < horizon; a += astep) {
        f(a);
    }
}


class QueueFunctionPair {
public:
    QueueFunctionPair(RandomQueueGenerator & rqg) : power(rqg.getRandomPower()) {
        proxys = rqg.createRandomQueue(power);
        recompute();
    }

    void createNTaskFunction(RandomQueueGenerator & rqg, unsigned int numTasks) {
        proxys = rqg.createNLengthQueue(numTasks, power);
        recompute();
    }

    double plotSampledGetMaxDifference(int n, std::ostream & os);

    double power;
    TaskProxy::List proxys;
    vector<double> lBounds;
    LAFunction function;
    double horizon;

private:
    void recompute() {
        proxys.getSwitchValues(lBounds);
        function = LAFunction(proxys, lBounds, power);
        horizon = function.getHorizon();
    }
};


double QueueFunctionPair::plotSampledGetMaxDifference(int n, std::ostream & os) {
    Time now = Time::getCurrentTime();
    double maxDiff = 0.0;

    forAinDomain(horizon*1.2, [&] (uint64_t a) {
        // Add a new task of length a
        if (!proxys.empty()) {
            vector<double> lBoundsTmp(lBounds);
            for (TaskProxy::List::iterator it = ++proxys.begin(); it != proxys.end(); ++it)
                if (it->a != a) {
                    double l = (now - it->rabs).seconds() / (it->a - a);
                    if (l > 0.0) {
                        lBoundsTmp.push_back(l);
                    }
                }
            std::sort(lBoundsTmp.begin(), lBoundsTmp.end());
            for (int i = 0; i < n; ++i)
                proxys.push_back(TaskProxy(a, power, now));
            proxys.sortMinSlowness(lBoundsTmp);
        } else
            for (int i = 0; i < n; ++i)
                proxys.push_back(TaskProxy(a, power, now));
        double estimate = function.estimateSlowness(a, n), real = 0.0;
        Time e = Time::getCurrentTime();
        // For each task, calculate finishing time
        for (TaskProxy::List::iterator i = proxys.begin(); i != proxys.end(); ++i) {
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
        if (difference > maxDiff) maxDiff = difference;
        os << a << ',' << estimate << ',' << real << ',' << difference << "  # ";
        for (TaskProxy::List::iterator i = proxys.begin(); i != proxys.end();) {
            os << i->id << ',';
            if (i->id == (unsigned int)-1)
                proxys.erase(i++);
            else ++i;
        }
        os << endl;
    } );
    return maxDiff;
}


/// Test Cases
struct LAFunctionFixture {
    LAFunctionFixture() : f(rqg) {
        TestHost::getInstance().reset();
    }

    RandomQueueGenerator rqg;
    QueueFunctionPair f;
};

BOOST_FIXTURE_TEST_SUITE(LAFunctionTest, LAFunctionFixture)

string plot(const LAFunction & f, double ah) {
    std::ostringstream oss;
    oss << "plot [" << LAFunction::minTaskLength << ':' << ah << "] ";
    const std::vector<std::pair<double, LAFunction::SubFunction> > & pieces = f.getPieces();
    for (unsigned int j = 0; j < pieces.size(); ++j) {
        const LAFunction::SubFunction & p = pieces[j].second;
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


BOOST_AUTO_TEST_CASE(LAFunction_copyconst) {
    BOOST_CHECK(!f.function.getPieces().empty());
    LAFunction copy(f.function);
    BOOST_CHECK_EQUAL(f.function, copy);
}


BOOST_AUTO_TEST_CASE(LAFunction_copyassign) {
    LAFunction copy;
    copy = f.function;
    BOOST_CHECK_EQUAL(f.function, copy);
}


BOOST_AUTO_TEST_CASE(LAFunction_moveconst) {
    LAFunction copy(f.function);
    LAFunction move(std::move(copy));
    BOOST_CHECK_EQUAL(f.function, move);
    BOOST_CHECK(copy.getPieces().empty());
}


BOOST_AUTO_TEST_CASE(LAFunction_moveassign) {
    LAFunction copy(f.function);
    LAFunction move;
    move = std::move(copy);
    BOOST_CHECK_EQUAL(f.function, move);
    BOOST_CHECK(copy.getPieces().empty());
}


BOOST_AUTO_TEST_CASE(LAFunction_estimateSlowness) {
    f.createNTaskFunction(rqg, 20);
    forAinDomain(f.horizon, [&] (uint64_t a) {
        // Check the estimation of one task
        BOOST_CHECK_CLOSE(f.function.getSlowness(a), f.function.estimateSlowness(a, 1), 0.01);
        BOOST_CHECK_LE(f.function.getSlowness(a), f.function.estimateSlowness(a, 1));
        BOOST_CHECK_LE(f.function.estimateSlowness(a, 1), f.function.estimateSlowness(a, 2));
        BOOST_CHECK_LE(f.function.estimateSlowness(a, 2), f.function.estimateSlowness(a, 3));
        BOOST_CHECK_LE(f.function.estimateSlowness(a, 3), f.function.estimateSlowness(a, 4));
        BOOST_CHECK_LE(f.function.estimateSlowness(a, 4), f.function.estimateSlowness(a, 5));
    } );
}


BOOST_AUTO_TEST_CASE(LAFunction_reduceMax) {
    LAFunction::setNumPieces(3);
    f.createNTaskFunction(rqg, 20);
    LAFunction fred(f.function);
    double accumAsqRed = 5 * fred.reduceMax(4, f.horizon);
    BOOST_CHECK_GE(accumAsqRed, 0);
    forAinDomain(f.horizon, [&] (uint64_t a) {
        BOOST_REQUIRE_GE(fred.getSlowness(a), f.function.getSlowness(a));
    } );
}


BOOST_AUTO_TEST_CASE(LAFunction_plotSampled) {
    Time now = TestHost::getInstance().getCurrentTime();
    ofstream ofs("laf_test.stat");
    ofs << "# F" << f.function << endl;
    ofs << "# Estimation with 1 task" << endl;
    double maxDiff = f.plotSampledGetMaxDifference(1, ofs);
    BOOST_CHECK_LE(maxDiff, 0.01);
    ofs << endl;
    if (!f.proxys.empty()) {
        double d = f.proxys.front().t;
        f.proxys.front().t = 0;
        TestHost::getInstance().setCurrentTime(now + Duration(d));
        ofs << "# Estimation with 1 task at the end of first task" << endl;
        f.plotSampledGetMaxDifference(1, ofs);
        ofs << endl;
        f.proxys.front().t = d;
    }
    TestHost::getInstance().setCurrentTime(now);
    for (int n = 2; n < 6; ++n) {
        ofs << "# Estimation with " << n << " tasks" << endl;
        f.plotSampledGetMaxDifference(n, ofs);
        ofs << endl;
    }
    ofs.close();
}


bool isMax(const LAFunction & f1,
        const LAFunction & f2,
        const LAFunction & max,
        uint64_t ah, uint64_t astep) {
    for (uint64_t a = LAFunction::minTaskLength; a < ah; a += astep) {
        double l1 = f1.getSlowness(a), l2 = f2.getSlowness(a), lmax = max.getSlowness(a);
        if ((l1 > l2 && lmax != l1) || (l1 <= l2 && lmax != l2))
            return false;
    }
    return true;
}


BOOST_AUTO_TEST_CASE(LAFunction_operations) {
    Time now = Time::getCurrentTime();
    //rqg.seed(1371033543);

    // Min/max and sum of several functions
    ofstream of("laf_test.ppl");
    for (int i = 0; i < 100; i++) {
        //LogMsg("Test.RI", INFO) << "Function " << i << ": ";
        QueueFunctionPair f11(rqg), f12(rqg), f13(rqg), f21(rqg), f22(rqg);
        double ah = 0.0;
        {
            if (f11.horizon > ah) ah = f11.horizon;
            if (f12.horizon > ah) ah = f12.horizon;
            if (f13.horizon > ah) ah = f13.horizon;
            if (f21.horizon > ah) ah = f21.horizon;
            if (f22.horizon > ah) ah = f22.horizon;
        }
        ah *= 1.2;
        uint64_t astep = (ah - LAFunction::minTaskLength) / 100;

        LAFunction min, max;
        min.min(f11.function, f12.function);
        min.min(min, f13.function);
        min.min(min, f21.function);
        min.min(min, f22.function);
        max.max(f11.function, f12.function);
        BOOST_CHECK_PREDICATE(isMax, (f11.function)(f12.function)(max)(ah)(astep));
        max.max(max, f13.function);
        BOOST_CHECK_PREDICATE(isMax, (f13.function)(max)(max)(ah)(astep));
        max.max(max, f21.function);
        BOOST_CHECK_PREDICATE(isMax, (f21.function)(max)(max)(ah)(astep));
        max.max(max, f22.function);
        BOOST_CHECK_PREDICATE(isMax, (f22.function)(max)(max)(ah)(astep));

        // Join f11 with f12
        LAFunction f112;
        double accumAsq112 = f112.maxAndLoss(f11.function, f12.function, 1, 1, LAFunction(), LAFunction(), ah);
        LAFunction accumAln112;
        //accumAln112.max(f11, f12);
        accumAln112.maxDiff(f11.function, f12.function, 1, 1, LAFunction(), LAFunction());
        BOOST_CHECK_PREDICATE(isMax, (f11.function)(f12.function)(f112)(ah)(astep));
        BOOST_CHECK_GE(accumAsq112, 0);
        BOOST_CHECK_CLOSE(accumAsq112, f112.sqdiff(f11.function, ah) + f112.sqdiff(f12.function, ah), 0.0001);
        BOOST_CHECK_CLOSE(accumAsq112, f11.function.sqdiff(f12.function, ah), 0.0001);

        // join f112 with f13, and that is f1
        LAFunction f1;
        double accumAsq1 = f1.maxAndLoss(f112, f13.function, 2, 1, accumAln112, LAFunction(), ah) + accumAsq112;
        LAFunction accumAln1;
        accumAln1.maxDiff(f112, f13.function, 2, 1, accumAln112, LAFunction());
        BOOST_CHECK_PREDICATE(isMax, (f112)(f13.function)(f1)(ah)(astep));
        BOOST_CHECK_GE(accumAsq1, 0);
        BOOST_CHECK_CLOSE(accumAsq1, f1.sqdiff(f11.function, ah) + f1.sqdiff(f12.function, ah) + f1.sqdiff(f13.function, ah), 0.0001);

        // join f21 with f22, and that is f2
        LAFunction f2;
        double accumAsq2 = f2.maxAndLoss(f21.function, f22.function, 1, 1, LAFunction(), LAFunction(), ah);
        LAFunction accumAln2;
        accumAln2.maxDiff(f21.function, f22.function, 1, 1, LAFunction(), LAFunction());
        BOOST_CHECK_PREDICATE(isMax, (f21.function)(f22.function)(f2)(ah)(astep));
        BOOST_CHECK_GE(accumAsq2, 0);
        BOOST_CHECK_CLOSE(accumAsq2, f2.sqdiff(f21.function, ah) + f2.sqdiff(f22.function, ah), 0.0001);

        // join f1 with f2, and that is f
        LAFunction f;
        double accumAsq = f.maxAndLoss(f1, f2, 3, 2, accumAln1, accumAln2, ah) + accumAsq1 + accumAsq2;
        LAFunction accumAln;
        accumAln.maxDiff(f1, f2, 3, 2, accumAln1, accumAln2);
        BOOST_CHECK_PREDICATE(isMax, (f1)(f2)(f)(ah)(astep));
        BOOST_CHECK_GE(accumAsq, 0);
        BOOST_CHECK_CLOSE(accumAsq, f.sqdiff(f11.function, ah)
                + f.sqdiff(f12.function, ah)
                + f.sqdiff(f13.function, ah)
                + f.sqdiff(f21.function, ah)
                + f.sqdiff(f22.function, ah), 0.0001);

        // Print functions
        of << "# Functions " << i << endl;
        of << "# F" << i << " f11: " << f11.function << endl << plot(f11.function, ah) << ", \"laf_test.stat\" i " << i << " e :::0::0 w lines" << endl;
        of << "# F" << i << " f12: " << f12.function << endl << plot(f12.function, ah) << endl;
        of << "# F" << i << " f112: " << f112 << endl << plot(f112, ah) << endl
                << "# accumAsq112 " << accumAsq112 << " =? " << (f112.sqdiff(f11.function, ah) + f112.sqdiff(f12.function, ah)) << endl;
        of << "# F" << i << " f13: " << f13.function << endl << plot(f13.function, ah) << endl;
        of << "# F" << i << " f1: " << f1 << endl << plot(f1, ah) << endl
                << "# accumAsq1 " << accumAsq1 << " =? " << (f1.sqdiff(f11.function, ah) + f1.sqdiff(f12.function, ah) + f1.sqdiff(f13.function, ah)) << endl;
        of << "# F" << i << " f21: " << f21.function << endl << plot(f21.function, ah) << endl;
        of << "# F" << i << " f22: " << f22.function << endl << plot(f22.function, ah) << endl;
        of << "# F" << i << " f2: " << f2 << endl << plot(f2, ah) << endl
                << "# accumAsq2 " << accumAsq2 << " =? " << (f2.sqdiff(f21.function, ah) + f2.sqdiff(f22.function, ah)) << endl;
        of << "# F" << i << " f: " << f << endl << plot(f, ah) << endl
                << "# accumAsq " << accumAsq << " =? " << (f.sqdiff(f11.function, ah) + f.sqdiff(f12.function, ah) + f.sqdiff(f13.function, ah) + f.sqdiff(f21.function, ah) + f.sqdiff(f22.function, ah)) << endl;
        of << "# F" << i << " min: " << min << endl << plot(min, ah) << endl;
        of << "# F" << i << " max: " << max << endl << plot(max, ah) << endl;
        of << endl;
    }
    of.close();
}

BOOST_AUTO_TEST_SUITE_END()   // LAFunctionTest

} // namespace stars



