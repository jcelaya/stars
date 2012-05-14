/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2010 Javier Celaya
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

#include <boost/test/unit_test.hpp>
#include <cmath>
#include "Logger.hpp"
#include "SerializableBatch.hpp"
#include "portable_binary_iarchive.hpp"
#include "portable_binary_oarchive.hpp"
#include <boost/archive/polymorphic_binary_oarchive.hpp>
#include <boost/archive/polymorphic_binary_iarchive.hpp>
using namespace std;
using namespace boost;


/// Test cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(Srlz)


/// Serialization
BOOST_AUTO_TEST_CASE(testSerializable) {
    // Test the serialization of doubles
    {
        stringstream ss;
        portable_binary_oarchive oa(ss);
        double test = 0.0;
        oa << test;
        test = -0.0;
        oa << test;
        test = INFINITY;
        oa << test;
        test = -INFINITY;
        oa << test;
        test = NAN;
        oa << test;
        test = random() * std::sqrt(2.0);
        oa << test;
        double d;
        portable_binary_iarchive ia(ss);
        ia >> d;
        BOOST_CHECK(fpclassify(d) == FP_ZERO && !signbit(d));
        ia >> d;
        BOOST_CHECK(fpclassify(d) == FP_ZERO && signbit(d));
        ia >> d;
        BOOST_CHECK(isinf(d) && !signbit(d));
        ia >> d;
        BOOST_CHECK(isinf(d) && signbit(d));
        ia >> d;
        BOOST_CHECK(isnan(d));
        ia >> d;
        BOOST_CHECK_EQUAL(d, test);
    }

    for (int i = 0; i < 10; i++) {
        stringstream ss;
        portable_binary_oarchive oa(ss);
        //boost::archive::polymorphic_binary_oarchive oa(ss);
        shared_ptr<BasicMsg> a(new SerializableBatch), c;
        oa << a;
        portable_binary_iarchive ia(ss);
        //boost::archive::polymorphic_binary_iarchive ia(ss);
        ia >> c;
        shared_ptr<SerializableBatch> b = dynamic_pointer_cast<SerializableBatch>(c);
        BOOST_REQUIRE(b.get());
        BOOST_CHECK(*static_pointer_cast<SerializableBatch>(a) == *b);
    }
}

BOOST_AUTO_TEST_SUITE_END()   // Srlz

BOOST_AUTO_TEST_SUITE_END()   // Cor
