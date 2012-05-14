/*
 *
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
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

#include <boost/test/floating_point_comparison.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <stdexcept>

#include "CheckMsg.hpp"
#include "InitStructNodeMsg.hpp"
#include "InsertMsg.hpp"
#include "UpdateZoneMsg.hpp"
#include "StrNodeNeededMsg.hpp"
#include "NewStrNodeMsg.hpp"
#include "NewChildMsg.hpp"
#include "NewFatherMsg.hpp"
#include "ZoneDescription.hpp"
using namespace std;
using namespace boost;


/// Test cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(StrMsg)

/// InitStructNodeMsg
BOOST_AUTO_TEST_CASE(testInitStructNodeMsg) {
    // Ctor
    InitStructNodeMsg i1, i2;
    shared_ptr<InitStructNodeMsg> p;

    // setLevel
    i1.setLevel(2);

    // getLevel
    BOOST_CHECK_EQUAL(i2.getLevel(), 0);
    BOOST_CHECK_EQUAL(i1.getLevel(), 2);

    // setParent
    CommAddress a1("127.0.0.1", 2030);
    i1.setFather(a1);

    // getParent
    BOOST_CHECK(!i2.isFatherValid());
    BOOST_CHECK(i1.isFatherValid());
    BOOST_CHECK(i1.getFather() == a1);

    // addChild
    CommAddress a2("127.0.0.2", 2030);
    CommAddress a3("127.0.0.3", 2030);
    CommAddress a4("127.0.0.4", 2030);
    i1.addChild(a2);
    i1.addChild(a3);
    i1.addChild(a4);

    // getChild
    BOOST_CHECK_THROW(i2.getChild(0), std::out_of_range);
    BOOST_CHECK_THROW(i1.getChild(-1), std::out_of_range);
    BOOST_CHECK_THROW(i1.getChild(3), std::out_of_range);
    BOOST_CHECK(i1.getChild(0) == CommAddress("127.0.0.2", 2030));
    BOOST_CHECK(i1.getChild(1) == CommAddress("127.0.0.3", 2030));
    BOOST_CHECK(i1.getChild(2) == CommAddress("127.0.0.4", 2030));

    // getNumChildren()
    BOOST_CHECK_EQUAL((int)i1.getNumChildren(), 3);

    // Serialization
    CheckMsgMethod::check(i1, p);
    InitStructNodeMsg & i3 = *p;
    BOOST_CHECK_EQUAL(i3.getNumChildren(), i1.getNumChildren());
    for (unsigned int i = 0; i < i3.getNumChildren(); i++) {
        BOOST_CHECK(i3.getChild(i) == i1.getChild(i));
    }
    BOOST_CHECK(i3.isFatherValid());
    BOOST_CHECK(i1.isFatherValid());
    BOOST_CHECK(i3.getFather() == i1.getFather());
    BOOST_CHECK_EQUAL(i3.getLevel(), i1.getLevel());
}

/// InsertMsg
BOOST_AUTO_TEST_CASE(testInsertMsg) {
    // Ctor
    InsertMsg i1, i2;

    // setWho
    CommAddress a1("127.0.0.1", 2030);
    i1.setWho(a1);

    // getWho
    BOOST_CHECK(i1.getWho() == CommAddress("127.0.0.1", 2030));

    // Serialization
    shared_ptr<InsertMsg> p;
    CheckMsgMethod::check(i1, p);
    InsertMsg & i3 = *p;
    BOOST_CHECK(i3.getWho() == i1.getWho());
}

/// ZoneDescription
BOOST_AUTO_TEST_CASE(testZoneDescription) {
    // Ctor
    ZoneDescription zone;

    // setMinAddress
    zone.setMinAddress(CommAddress("127.0.0.1", 2030));

    // getMinAddress
    BOOST_CHECK_EQUAL(zone.getMinAddress(), CommAddress("127.0.0.1", 2030));

    // setMaxAddress
    zone.setMaxAddress(CommAddress("127.0.0.2", 2030));

    // getMaxAddress
    BOOST_CHECK_EQUAL(zone.getMaxAddress(), CommAddress("127.0.0.2", 2030));

    // setAvailableStrNodes
    zone.setAvailableStrNodes(4);

    // getAvailableStrNodes
    BOOST_CHECK_EQUAL(zone.getAvailableStrNodes(), 4);

    // operator ==
    shared_ptr<ZoneDescription> zone2(new ZoneDescription(zone));
    BOOST_CHECK_EQUAL(zone, *zone2);

    // contains
    BOOST_CHECK(zone.contains(CommAddress("127.0.0.1", 2030)));
    BOOST_CHECK(zone.contains(CommAddress("127.0.0.2", 2030)));

    // distance
    BOOST_CHECK_CLOSE(zone.distance(CommAddress("127.0.0.1", 2030)), 0.0, 0.1);
    BOOST_CHECK_CLOSE(zone.distance(CommAddress("127.0.0.4", 2030)), 2.0, 0.001);

    // aggregate
    zone2->setMinAddress(CommAddress("127.0.0.5", 2030));
    zone2->setMaxAddress(CommAddress("127.0.0.7", 2030));
    shared_ptr<ZoneDescription> zone3(new ZoneDescription(zone));
    list<shared_ptr<ZoneDescription> > zones;
    zones.push_back(zone2);
    zones.push_back(zone3);
    zone.aggregate(zones);
    BOOST_CHECK_EQUAL(zone.getMinAddress(), CommAddress("127.0.0.1", 2030));
    BOOST_CHECK_EQUAL(zone.getMaxAddress(), CommAddress("127.0.0.7", 2030));

    // Serialization
    stringstream ss;
    portable_binary_oarchive oa(ss);
    shared_ptr<ZoneDescription> ee(new ZoneDescription(zone));
    oa << ee;
    BOOST_TEST_MESSAGE("ZoneDescription size of " << ss.str().size() << " bytes.");
    portable_binary_iarchive ia(ss);
    shared_ptr<ZoneDescription> zone4;
    ia >> zone4;
    BOOST_CHECK_EQUAL(zone, *zone4);
}

/// UpdateMsg
BOOST_AUTO_TEST_CASE(testUpdateZoneMsg) {
    // Ctor
    UpdateZoneMsg i1, i2;

    // setZone
    shared_ptr<ZoneDescription> zone(new ZoneDescription());
    zone->setMinAddress(CommAddress("127.0.0.1", 2030));
    zone->setMaxAddress(CommAddress("127.0.0.2", 2030));
    zone->setAvailableStrNodes(4);
    i1.setZone(zone);

    // getZone
    BOOST_REQUIRE(i1.getZone().get() != NULL);
    BOOST_CHECK(*i1.getZone() == *zone);

    // Serialization
    shared_ptr<UpdateZoneMsg> p;
    CheckMsgMethod::check(i1, p);
    UpdateZoneMsg & i3 = *p;
    BOOST_REQUIRE(i3.getZone().get() != NULL);
    BOOST_CHECK(*i3.getZone() == *i1.getZone());
}

/// StrNodeNeededMsg
BOOST_AUTO_TEST_CASE(testStrNodeNeededMsg) {
    // Ctor
    StrNodeNeededMsg i1, i2;

    // setWhoNeeds
    CommAddress a1("127.0.0.1", 2030);
    i1.setWhoNeeds(a1);

    // getWhoNeeds
    BOOST_CHECK(i1.getWhoNeeds() == CommAddress("127.0.0.1", 2030));

    // Serialization
    shared_ptr<StrNodeNeededMsg> p;
    CheckMsgMethod::check(i1, p);
    StrNodeNeededMsg & i3 = *p;
    BOOST_CHECK(i3.getWhoNeeds() == i1.getWhoNeeds());
}

/// NewStrNodeMsg
BOOST_AUTO_TEST_CASE(testNewStrNodeMsg) {
    // Ctor
    NewStrNodeMsg i1, i2;

    // setWhoOffers
    CommAddress a1("127.0.0.1", 2030);
    i1.setWhoOffers(a1);

    // getWhoOffers
    BOOST_CHECK(i1.getWhoOffers() == CommAddress("127.0.0.1", 2030));

    // Serialization
    shared_ptr<NewStrNodeMsg> p;
    CheckMsgMethod::check(i1, p);
    NewStrNodeMsg & i3 = *p;
    BOOST_CHECK(i3.getWhoOffers() == i1.getWhoOffers());
}

/// NewChildMsg
BOOST_AUTO_TEST_CASE(testNewChildMsg) {
    // Ctor
    NewChildMsg i1, i2;

    // setChild
    CommAddress a1("127.0.0.1", 2030);
    i1.setChild(a1);

    // getChild
    BOOST_CHECK(i1.getChild() == CommAddress("127.0.0.1", 2030));

    // setSequence
    i1.setSequence(23453);

    // getSequence
    BOOST_CHECK(i1.getSequence() == 23453);

    // replaces
    i1.replaces(true);
    BOOST_CHECK(i1.replaces());
    i1.replaces(false);
    BOOST_CHECK(!i1.replaces());

    // Serialization
    shared_ptr<NewChildMsg> p;
    CheckMsgMethod::check(i1, p);
    NewChildMsg & i3 = *p;
    BOOST_CHECK(i3.getSequence() == 23453);
    BOOST_CHECK(!i3.replaces());
    BOOST_CHECK(i3.getChild() == i1.getChild());
}

/// NewFatherMsg
BOOST_AUTO_TEST_CASE(testNewFatherMsg) {
    // Ctor
    NewFatherMsg i1, i2;

    // setFather
    CommAddress a1("127.0.0.1", 2030);
    i1.setFather(a1);

    // getFather
    BOOST_CHECK(i1.getFather() == CommAddress("127.0.0.1", 2030));

    // Serialization
    shared_ptr<NewFatherMsg> p;
    CheckMsgMethod::check(i1, p);
    NewFatherMsg & i3 = *p;
    BOOST_CHECK(i3.getFather() == i1.getFather());
}

BOOST_AUTO_TEST_SUITE_END()   // StrMsg

BOOST_AUTO_TEST_SUITE_END()   // Cor
