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
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "ScalarParameter.hpp"


struct ScalarParameterFixture {
    ScalarParameterFixture() : min1(MIN_VALUE + 1), min2(MAX_VALUE - 1){
        range.setLimits(MIN_VALUE);
        range.extend(MAX_VALUE);
        unitary.extend(1);
    }

    enum {
        MIN_VALUE = 500,
        MAX_VALUE = 1000,
    };

    MinParameter<int, int> min0, min1, min2;
    Interval<int, int> range, unitary;
};

#define sq(x) (x)*(x)
#define min(x, y) (x < y ? x : y)

BOOST_FIXTURE_TEST_SUITE(ScalarParameterTest, ScalarParameterFixture)

BOOST_AUTO_TEST_CASE(buildScalarParameter) {
    BOOST_CHECK_EQUAL(min0.getValue(), 0);
    BOOST_CHECK_EQUAL(min1.getValue(), MIN_VALUE + 1);
    BOOST_CHECK_EQUAL(min2.getValue(), MAX_VALUE - 1);
}

BOOST_AUTO_TEST_CASE(far) {
    BOOST_CHECK_EQUAL(min1.getInterval(range, 2), 0);
    BOOST_CHECK_EQUAL(min2.getInterval(range, 2), 1);
    BOOST_CHECK(min1.far(min2, range, 2));
    BOOST_CHECK(!min1.far(min2, range, 1));
}

BOOST_AUTO_TEST_CASE(aggregateAndMSE) {
    boost::random::mt19937 gen;
    boost::random::uniform_int_distribution<> dist(MIN_VALUE, MAX_VALUE);

    for (int i = 0; i < 100; ++i) {
        int v1 = dist(gen), v2 = dist(gen), v3 = dist(gen), v4 = dist(gen), v5 = dist(gen);
        int minimum = min(v1, v2);
        MinParameter<int, int> p1(v1), p2(v2), p3(v3), p4(v4), p5(v5);
        p1.aggregate(1, p2, 1);
        BOOST_CHECK_EQUAL(p1.norm(unitary, 1), sq(p1.getValue() - v1) + sq(p1.getValue() - v2));
        BOOST_CHECK_EQUAL(p1.getValue(), minimum);
        p3.aggregate(1, p4, 1);
        BOOST_CHECK_EQUAL(p3.norm(unitary, 1), sq(p3.getValue() - v3) + sq(p3.getValue() - v4));
        p3.aggregate(2, p5, 1);
        BOOST_CHECK_EQUAL(p3.norm(unitary, 1), sq(p3.getValue() - v3) + sq(p3.getValue() - v4) + sq(p3.getValue() - v5));
        p1.aggregate(2, p3, 3);
        BOOST_CHECK_EQUAL(p1.norm(unitary, 1), sq(p1.getValue() - v1) + sq(p1.getValue() - v2) + sq(p1.getValue() - v3) + sq(p1.getValue() - v4) + sq(p1.getValue() - v5));
        minimum = min(minimum, v3);
        minimum = min(minimum, v4);
        minimum = min(minimum, v5);
        BOOST_CHECK_EQUAL(p1.getValue(), minimum);
    }
}

BOOST_AUTO_TEST_SUITE_END()
