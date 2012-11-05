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
#include <cmath>
#include "Logger.hpp"
#include "SerializableBatch.hpp"
using namespace std;
using namespace boost;


/// Test cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(Srlz)


/// Serialization
BOOST_AUTO_TEST_CASE(testSerializable) {
    for (int i = 0; i < 10; i++) {
        std::stringstream buffer;
        msgpack::packer<std::ostream> pk(&buffer);
        BasicMsg * a(new SerializableBatch), * c;
        a->pack(pk);
        msgpack::unpacker pac;
        pac.reserve_buffer(buffer.tellp());
        buffer.readsome(pac.buffer(), buffer.tellp());
        pac.buffer_consumed(buffer.tellp());
        c = BasicMsg::unpackMessage(pac);
        SerializableBatch * b = dynamic_cast<SerializableBatch *>(c);
        BOOST_REQUIRE(b);
        BOOST_CHECK(*static_cast<SerializableBatch *>(a) == *b);
        delete a;
        delete c;
    }
}

BOOST_AUTO_TEST_SUITE_END()   // Srlz

BOOST_AUTO_TEST_SUITE_END()   // Cor
