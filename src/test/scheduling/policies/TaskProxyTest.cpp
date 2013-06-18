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
#include "TaskProxy.hpp"
#include "FSPTaskList.hpp"
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

BOOST_AUTO_TEST_CASE(buildTaskProxy) {
    Time now((int64_t)2000000), deadline(120000000);
    TaskProxy tp(1000.0, 2000.0, now);
    tp.setSlowness(0.118);
    BOOST_CHECK_EQUAL(tp.d, deadline);
}

FSPTaskList getTestList() {
    double power = 1000.0;
    Time now((int64_t) 0);
    FSPTaskList l;
    l.addTasks(TaskProxy(335000, power, now + Duration(-1005.0)));
    l.addTasks(TaskProxy(25000, power, now + Duration(-1000.0)));
    l.addTasks(TaskProxy(20000, power, now + Duration(-500.0)));
    l.addTasks(TaskProxy(20000, power, now + Duration(-500.0)));
    l.addTasks(TaskProxy(15000, power, now + Duration(-250.0)));
    l.addTasks(TaskProxy(10000, power, now + Duration(-125.0)));
    unsigned int id = 0;
    for (auto & i: l)
        i.id = ++id;
    return l;
}

BOOST_AUTO_TEST_CASE(buildFSPTaskList) {
    FSPTaskList l;
    BOOST_CHECK(l.empty());
}

BOOST_AUTO_TEST_CASE(sortFSPTaskList) {
    Time now = TestHost::getInstance().getCurrentTime();
    RandomQueueGenerator rqg;
    for (int i = 0; i < 10; ++i) {
        FSPTaskList l(std::move(rqg.createRandomQueue()));
        l.sortBySlowness(0.2);
        Time d = l.front().rabs;
        for (FSPTaskList::const_iterator it = l.begin(); it != l.end(); ++it) {
            BOOST_CHECK_LE(d, it->d);
            d = it->d;
        }
    }
}

BOOST_AUTO_TEST_CASE(FSPTaskList_getSlowness) {
    FSPTaskList l = getTestList();
    BOOST_CHECK_EQUAL(l.getSlowness(), 0.055);
}

BOOST_AUTO_TEST_CASE(FSPTaskList_meetDeadlines) {
    FSPTaskList l = getTestList();
    BOOST_CHECK(l.meetDeadlines(0.06, TestHost::getInstance().getCurrentTime()));
    BOOST_CHECK(!l.meetDeadlines(0.05, TestHost::getInstance().getCurrentTime()));
}

BOOST_AUTO_TEST_CASE(FSPTaskList_addTasks) {
    FSPTaskList l = getTestList();
    const std::vector<double> & boundaries = l.getBoundaries();
    BOOST_REQUIRE_EQUAL(boundaries.size(), 7);
    BOOST_CHECK_EQUAL(boundaries[0], 0.004);
    BOOST_CHECK_EQUAL(boundaries[1], 0.025);
    BOOST_CHECK_EQUAL(boundaries[2], 0.0375);
    BOOST_CHECK_EQUAL(boundaries[3], 0.05);
    BOOST_CHECK_EQUAL(boundaries[4], 875.0/15000.0);
    BOOST_CHECK_EQUAL(boundaries[5], 0.075);
    BOOST_CHECK_EQUAL(boundaries[6], 0.1);
}

BOOST_AUTO_TEST_CASE(FSPTaskList_computeBoundaries) {
    FSPTaskList l = getTestList();
    l.removeTask(5);
    const std::vector<double> & boundaries = l.getBoundaries();
    BOOST_REQUIRE_EQUAL(boundaries.size(), 4);
    BOOST_CHECK_EQUAL(boundaries[0], 0.004);
    BOOST_CHECK_EQUAL(boundaries[1], 0.0375);
    BOOST_CHECK_EQUAL(boundaries[2], 875.0/15000.0);
    BOOST_CHECK_EQUAL(boundaries[3], 0.1);
}

void checkMinSlownessOrder(FSPTaskList proxys) {
    if (proxys.empty()) return;
    proxys.sortMinSlowness();
    double slowness = proxys.getSlowness();
    double power = proxys.front().getEffectiveSpeed();
    std::ostringstream oss;
    oss.precision(12);
    for (FSPTaskList::iterator it = proxys.begin(); it != proxys.end(); ++it) {
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
            for (FSPTaskList::iterator it = proxys.begin(); it != proxys.end(); ++it) {
                std::cout << *it;
            }
            std::cout << std::endl;
        }
    } while (std::next_permutation(++proxys.begin(), proxys.end(), orderById));
}

/// SlownessAlgorithm
BOOST_AUTO_TEST_CASE(FSPTaskList_sortMinSlowness) {
    TestHost::getInstance().reset();
    RandomQueueGenerator rqg;
    //rqg.seed(1371024975);

    checkMinSlownessOrder(getTestList());

    for (int numTests = 1 << 8, i = 1; numTests > 0; numTests >>= 1, ++i) {
        for (int j = 0; j < numTests; ++j) {
            FSPTaskList proxys(std::move(rqg.createNLengthQueue(i)));
            checkMinSlownessOrder(proxys);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()   // TaskProxyTest

}
