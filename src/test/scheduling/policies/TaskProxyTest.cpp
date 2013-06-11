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
#include <ctime>
#include "TaskProxy.hpp"
#include "TestHost.hpp"
#include "RandomQueueGenerator.hpp"


namespace stars {

/// Test Cases
BOOST_AUTO_TEST_SUITE(TaskProxyTest)

std::ostream & operator<<(std::ostream & os, const TaskProxy & o) {
    return os << '(' << (o.rabs.getRawDate() / 1000000.0) << ':' << o.a << ')';
}

bool orderById(const TaskProxy & l, const TaskProxy & r) {
    return l.id < r.id;
}

TaskProxy::List getTestList() {
    double power = 1000.0;
    Time now((int64_t) 0);
    TaskProxy::List l;
    l.push_back(TaskProxy(335000, power, now + Duration(-1005.0)));
    l.push_back(TaskProxy(25000, power, now + Duration(-1000.0)));
    l.push_back(TaskProxy(20000, power, now + Duration(-500.0)));
    l.push_back(TaskProxy(20000, power, now + Duration(-500.0)));
    l.push_back(TaskProxy(15000, power, now + Duration(-250.0)));
    l.push_back(TaskProxy(10000, power, now + Duration(-125.0)));
    return l;
}

BOOST_AUTO_TEST_CASE(buildTaskProxy) {
    Time now((int64_t)2000000), deadline(120000000);
    TaskProxy tp(1000.0, 2000.0, now);
    tp.setSlowness(0.118);
    BOOST_CHECK_EQUAL(tp.d, deadline);
}

BOOST_AUTO_TEST_CASE(buildTaskProxyList) {
    TaskProxy::List l;
    BOOST_CHECK(l.empty());
}

BOOST_AUTO_TEST_CASE(sortTaskProxyList) {
    Time now = TestHost::getInstance().getCurrentTime();
    RandomQueueGenerator & rqg = RandomQueueGenerator::getInstance();
    for (int i = 0; i < 10; ++i) {
        TaskProxy::List l = rqg.createRandomQueue();
        l.sortBySlowness(0.2);
        Time d = l.front().rabs;
        for (TaskProxy::List::const_iterator it = l.begin(); it != l.end(); ++it) {
            BOOST_CHECK_LE(d, it->d);
            d = it->d;
        }
    }
}

BOOST_AUTO_TEST_CASE(TaskProxyList_getSlowness) {
    TaskProxy::List l = getTestList();
    BOOST_CHECK_EQUAL(l.getSlowness(), 0.055);
}

BOOST_AUTO_TEST_CASE(TaskProxyList_meetDeadlines) {
    TaskProxy::List l = getTestList();
    BOOST_CHECK(l.meetDeadlines(0.06, TestHost::getInstance().getCurrentTime()));
    BOOST_CHECK(!l.meetDeadlines(0.05, TestHost::getInstance().getCurrentTime()));
}

BOOST_AUTO_TEST_CASE(TaskProxyList_getSwitchValues) {
    TaskProxy::List l = getTestList();
    std::vector<double> sValues;
    l.getSwitchValues(sValues);
    BOOST_REQUIRE_EQUAL(sValues.size(), 7);
    BOOST_CHECK_EQUAL(sValues[0], 0.004);
    BOOST_CHECK_EQUAL(sValues[1], 0.025);
    BOOST_CHECK_EQUAL(sValues[2], 0.0375);
    BOOST_CHECK_EQUAL(sValues[3], 0.05);
    BOOST_CHECK_EQUAL(sValues[4], 875.0/15000.0);
    BOOST_CHECK_EQUAL(sValues[5], 0.075);
    BOOST_CHECK_EQUAL(sValues[6], 0.1);
}

void checkMinSlownessOrder(TaskProxy::List proxys, const std::vector<double> & lBounds) {
    if (proxys.empty()) return;
    double slowness = proxys.getSlowness();
    double power = proxys.front().getEffectiveSpeed();
    std::ostringstream oss;
    oss.precision(12);
    for (TaskProxy::List::iterator it = proxys.begin(); it != proxys.end(); ++it) {
        oss << *it;
    }
    // Check that no other sorting produces a lower slowness.
    proxys.sort(orderById);
    do {
        double s = proxys.getSlowness();
        BOOST_CHECK_LE(slowness, s);
        if (slowness > s) {
            std::cout << "Sorted  queue: " << power << ' ' << oss.str() << std::endl;
            std::cout << "Current queue: " << power << ' ';
            for (TaskProxy::List::iterator it = proxys.begin(); it != proxys.end(); ++it) {
                std::cout << *it;
            }
            std::cout << std::endl;
        }
    } while (std::next_permutation(++proxys.begin(), proxys.end(), orderById));
}

/// SlownessAlgorithm
BOOST_AUTO_TEST_CASE(TaskProxyList_sortMinSlowness) {
    TestHost::getInstance().reset();
    RandomQueueGenerator & rqg = RandomQueueGenerator::getInstance();
    time_t seed = std::time(NULL);
    //seed = 1370884902;
    std::cout << "Using seed " << seed << std::endl;
    rqg.seed(seed);
    std::vector<double> lBounds;
    TaskProxy::List proxys;

    {
        proxys = getTestList();
        proxys.getSwitchValues(lBounds);
        proxys.sortMinSlowness(lBounds);
        checkMinSlownessOrder(proxys, lBounds);
    }

    for (int numTests = 1 << 8, i = 1; numTests > 0; numTests >>= 1, ++i) {
        for (int j = 0; j < numTests; ++j) {
            proxys = rqg.createNLengthQueue(i);
            proxys.getSwitchValues(lBounds);
            proxys.sortMinSlowness(lBounds);
            checkMinSlownessOrder(proxys, lBounds);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()   // TaskProxyTest

}
