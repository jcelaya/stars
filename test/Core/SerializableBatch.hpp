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

#ifndef SERIALIZABLEBATCH_HPP_
#define SERIALIZABLEBATCH_HPP_

#include <cstdlib>
#include <cmath>
#include <boost/test/unit_test.hpp>
#include "BasicMsg.hpp"


class SerializableBatch : public BasicMsg {
public:
    MSGPACK_DEFINE(u8, u16, u32, u64, i8, i16, i32, i64, b, d, v, l, p, s);
    
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    bool b;
    double d;
    std::vector<uint32_t> v;
    std::list<double> l;
    std::pair<uint8_t, int16_t> p;
    std::string s;

    MESSAGE_SUBCLASS(SerializableBatch);
    
    static int random() {
        return std::rand() - RAND_MAX / 2;
    }

    SerializableBatch() : u8(random()), u16(random()), u32(random()), u64(random()),
            i8(random()), i16(random()), i32(random()), i64(random()), b(random() > 0),
            d(random() * std::sqrt(2.0)), v(3, random()), l(2, random() * std::sqrt(2.0)),
            p(std::make_pair(random(), random())), s(5, (char)random()) {}

    bool operator==(const SerializableBatch & r) {
        bool result = true;
        BOOST_CHECK_EQUAL(u8, r.u8);
        result = result && u8 == r.u8;
        BOOST_CHECK_EQUAL(u16, r.u16);
        result = result && u16 == r.u16;
        BOOST_CHECK_EQUAL(u32, r.u32);
        result = result && u32 == r.u32;
        BOOST_CHECK_EQUAL(u64, r.u64);
        result = result && u64 == r.u64;
        BOOST_CHECK_EQUAL(i8, r.i8);
        result = result && i8 == r.i8;
        BOOST_CHECK_EQUAL(i16, r.i16);
        result = result && i16 == r.i16;
        BOOST_CHECK_EQUAL(i32, r.i32);
        result = result && i32 == r.i32;
        BOOST_CHECK_EQUAL(i64, r.i64);
        result = result && i64 == r.i64;
        BOOST_CHECK_EQUAL(b, r.b);
        result = result && b == r.b;
        BOOST_CHECK_EQUAL(d, r.d);
        result = result && d == r.d;
        BOOST_CHECK(v == r.v);
        result = result && v == r.v;
        BOOST_CHECK(l == r.l);
        result = result && l == r.l;
        BOOST_CHECK(p == r.p);
        result = result && p == r.p;
        BOOST_CHECK_EQUAL(s, r.s);
        result = result && s == r.s;
        return result;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}
};

#endif /* SERIALIZABLEBATCH_HPP_ */
