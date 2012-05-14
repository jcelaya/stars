/*
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

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include "CommLayer.hpp"
#include "BasicMsg.hpp"
#include "Time.hpp"
#include "TestHost.hpp"
#include "BoostSerializationArchiveExport.hpp"
#include "Logger.hpp"
using namespace std;
using namespace boost;


/// Test objects

class Ping : public BasicMsg {
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(BasicMsg);
    }

public:
    Ping() {}

    Ping * clone() const {
        return new Ping(*this);
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("Ping");
    }
};


class Pong : public BasicMsg {
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(BasicMsg);
    }

public:
    Pong() {}

    Pong * clone() const {
        return new Pong(*this);
    }

    // This is documented in BasicMsg
    void output(std::ostream & os) const {}

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("Pong");
    }
};

BOOST_CLASS_EXPORT(Ping);
BOOST_CLASS_EXPORT(Pong);


// A service that does pings
class PingService : public Service {
public:

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg) {
        if (typeid(msg) == typeid(Ping)) {
            handle(src, static_cast<const Ping &>(msg));
            return true;
        } else return false;
    }

    void handle(const CommAddress & src, const Ping & msg) {
        CommLayer::getInstance().sendMessage(src, new Pong);
    }
};


// A service that does pongs
class PongService : public Service {
    bool pinged;

public:

    PongService() : pinged(false) {}

    bool receiveMessage(const CommAddress & src, const BasicMsg & msg) {
        if (typeid(msg) == typeid(Pong)) {
            handle(src, static_cast<const Pong &>(msg));
            return true;
        } else return false;
    }

    void handle(const CommAddress & src, const Pong & msg) {
        pinged = true;
    }

    void ping(const CommAddress & remoteAddress) {
        pinged = false;
        CommLayer::getInstance().sendMessage(remoteAddress, new Ping);
    }

    void pingLocal() {
        pinged = false;
        CommLayer::getInstance().sendLocalMessage(new Ping);
    }

    bool isPinged() const {
        return pinged;
    }
};


/// Test cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(Comm)


/// CommLayer Scenario
BOOST_AUTO_TEST_CASE(testCommAddress) {
    // Test the CommAddress
    CommAddress a1("127.0.0.1", 2030), a2("127.0.0.2", 2030), a3(237486, 2030);
    BOOST_CHECK(a1 == a1);
    BOOST_CHECK(a1 <= a2);
    BOOST_CHECK(a1.getIPString() == string("127.0.0.1"));
    BOOST_CHECK(a1.getIPNum() == 2130706433U);
    a2 = a1;
    BOOST_CHECK(a2.getIPString() == string("127.0.0.1"));
    BOOST_CHECK(a3.getIPNum() == 237486);
    BOOST_CHECK(a3.getIPString() == string("0.3.159.174"));
}

BOOST_AUTO_TEST_CASE(testCommLayerLocal) {
    TestHost::getInstance().reset();
    ConfigurationManager::getInstance().setPort(2030);

    PingService s1;
    PongService s2;

    CommAddress a1 = CommLayer::getInstance().getLocalAddress();
    s2.ping(a1);
    CommLayer::getInstance().processNextMessage();
    CommLayer::getInstance().processNextMessage();
    BOOST_CHECK(s2.isPinged());
    s2.pingLocal();
    CommLayer::getInstance().processNextMessage();
    CommLayer::getInstance().processNextMessage();
    BOOST_CHECK(s2.isPinged());
}

void pingThread() {
    TestHost::getInstance().addSingleton();
    ConfigurationManager::getInstance().setPort(2040);
    CommLayer::getInstance().listen();

    shared_ptr<PongService> s2(new PongService);
    CommAddress a1(CommLayer::getInstance().getLocalAddress().getIP(), 2030);
    s2->ping(a1);
    CommLayer::getInstance().processNextMessage();
    BOOST_CHECK(s2->isPinged());
}

BOOST_AUTO_TEST_CASE(testCommLayerRemote) {
    TestHost::getInstance().reset();
    ConfigurationManager::getInstance().setPort(2030);
    CommLayer::getInstance().listen();

    shared_ptr<PingService> s1(new PingService);

    // Unlock the other process
    thread t(pingThread);
    CommLayer::getInstance().processNextMessage();
    t.join();
}

BOOST_AUTO_TEST_SUITE_END()   // Comm

BOOST_AUTO_TEST_SUITE_END()   // Cor