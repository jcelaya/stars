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

#include <list>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include "CheckMsg.hpp"
#include "DPAvailabilityInformation.hpp"
#include "TestHost.hpp"
using namespace std;


BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(DPAvailabilityInformationTest)

/// TaskEventMsg
BOOST_AUTO_TEST_CASE(DPAvailabilityInformationTest_checkMsg) {
    TestHost::getInstance().reset();

    // Ctor
    DPAvailabilityInformation e;
    std::shared_ptr<DPAvailabilityInformation> p;

    // TODO: Check other things

    CheckMsgMethod::check(e, p);
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor
