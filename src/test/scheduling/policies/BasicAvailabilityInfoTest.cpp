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
#include "CheckMsg.hpp"
#include "IBPAvailabilityInformation.hpp"
using namespace boost;


BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(aiTS)

/// TaskEventMsg
BOOST_AUTO_TEST_CASE(baiMsg) {
    // Ctor
    IBPAvailabilityInformation e;
    shared_ptr<IBPAvailabilityInformation> p;

    // TODO: Check other things

    CheckMsgMethod::check(e, p);
}

BOOST_AUTO_TEST_SUITE_END()   // aiTS

BOOST_AUTO_TEST_SUITE_END()   // Cor
