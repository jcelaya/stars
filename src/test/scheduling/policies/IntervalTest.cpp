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
#include "Interval.hpp"
using namespace stars;

struct IntervalFixture {
    IntervalFixture() {
        interval1.setLimits(BASE);
        interval2.setLimits(BASEUP);
        interval2.extend(BASEDOWN);
    }

    enum {
        BASE = 5,
        BASEUP = 10,
        BASEDOWN = -3,
        UP = 15,
        DOWN = -6,
        IN = 0,
    };

    Interval<int> interval0, interval1, interval2, extension;
};

BOOST_FIXTURE_TEST_SUITE(IntervalTest, IntervalFixture)

BOOST_AUTO_TEST_CASE(buildInterval) {
    BOOST_CHECK_EQUAL(interval0.getMin(), 0);
    BOOST_CHECK_EQUAL(interval0.getMax(), 0);
    BOOST_CHECK(interval0.empty());
}

BOOST_AUTO_TEST_CASE(setMaximumMinimum) {
    interval0.setMaximum(-1);
    BOOST_CHECK_EQUAL(interval0.getMin(), -1);
    BOOST_CHECK_EQUAL(interval0.getMax(), -1);
    interval0.setMinimum(-3);
    BOOST_CHECK_EQUAL(interval0.getMin(), -3);
    BOOST_CHECK_EQUAL(interval0.getMax(), -1);
}

BOOST_AUTO_TEST_CASE(setMinimumMaximum) {
    interval0.setMinimum(1);
    BOOST_CHECK_EQUAL(interval0.getMin(), 1);
    BOOST_CHECK_EQUAL(interval0.getMax(), 1);
    interval0.setMaximum(3);
    BOOST_CHECK_EQUAL(interval0.getMin(), 1);
    BOOST_CHECK_EQUAL(interval0.getMax(), 3);
}

BOOST_AUTO_TEST_CASE(extentOneValue) {
    BOOST_CHECK_EQUAL(interval1.getMin(), BASE);
    BOOST_CHECK_EQUAL(interval1.getMax(), BASE);
    BOOST_CHECK_EQUAL(interval1.getExtent(), 0);
}

BOOST_AUTO_TEST_CASE(extentUp) {
    interval1.extend(BASEUP);
    BOOST_CHECK_EQUAL(interval1.getMin(), BASE);
    BOOST_CHECK_EQUAL(interval1.getMax(), BASEUP);
    BOOST_CHECK_EQUAL(interval1.getExtent(), BASEUP - BASE);
}

BOOST_AUTO_TEST_CASE(extentDown) {
    interval1.extend(BASEDOWN);
    BOOST_CHECK_EQUAL(interval1.getMin(), BASEDOWN);
    BOOST_CHECK_EQUAL(interval1.getMax(), BASE);
    BOOST_CHECK_EQUAL(interval1.getExtent(), BASE - BASEDOWN);
}

BOOST_AUTO_TEST_CASE(extentIntervalDownDown) {
    extension.setLimits(DOWN);
    extension.extend(BASEDOWN);
    interval2.extend(extension);
    BOOST_CHECK_EQUAL(interval2.getMin(), DOWN);
    BOOST_CHECK_EQUAL(interval2.getMax(), BASEUP);
    BOOST_CHECK_EQUAL(interval2.getExtent(), BASEUP - DOWN);
}

BOOST_AUTO_TEST_CASE(extentIntervalDownIn) {
    extension.setLimits(DOWN);
    extension.extend(IN);
    interval2.extend(extension);
    BOOST_CHECK_EQUAL(interval2.getMin(), DOWN);
    BOOST_CHECK_EQUAL(interval2.getMax(), BASEUP);
    BOOST_CHECK_EQUAL(interval2.getExtent(), BASEUP - DOWN);
}

BOOST_AUTO_TEST_CASE(extentIntervalInIn) {
    extension.setLimits(IN);
    interval2.extend(extension);
    BOOST_CHECK_EQUAL(interval2.getMin(), BASEDOWN);
    BOOST_CHECK_EQUAL(interval2.getMax(), BASEUP);
    BOOST_CHECK_EQUAL(interval2.getExtent(), BASEUP - BASEDOWN);
}

BOOST_AUTO_TEST_CASE(extentIntervalInUp) {
    extension.setLimits(IN);
    extension.extend(UP);
    interval2.extend(extension);
    BOOST_CHECK_EQUAL(interval2.getMin(), BASEDOWN);
    BOOST_CHECK_EQUAL(interval2.getMax(), UP);
    BOOST_CHECK_EQUAL(interval2.getExtent(), UP - BASEDOWN);
}

BOOST_AUTO_TEST_CASE(extentIntervalUpUp) {
    extension.setLimits(BASEUP);
    extension.extend(UP);
    interval2.extend(extension);
    BOOST_CHECK_EQUAL(interval2.getMin(), BASEDOWN);
    BOOST_CHECK_EQUAL(interval2.getMax(), UP);
    BOOST_CHECK_EQUAL(interval2.getExtent(), UP - BASEDOWN);
}

BOOST_AUTO_TEST_CASE(extentIntervalDownUp) {
    extension.setLimits(DOWN);
    extension.extend(UP);
    interval2.extend(extension);
    BOOST_CHECK_EQUAL(interval2.getMin(), DOWN);
    BOOST_CHECK_EQUAL(interval2.getMax(), UP);
    BOOST_CHECK_EQUAL(interval2.getExtent(), UP - DOWN);
}

BOOST_AUTO_TEST_SUITE_END()
