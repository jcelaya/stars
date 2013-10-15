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
#include "CheckMsg.hpp"
#include "TestHost.hpp"
#include "FSPAvailabilityInformation.hpp"
#include "../RandomQueueGenerator.hpp"
using namespace std;


namespace stars {

struct FSPAvailabilityInfoFixture {
    FSPAvailabilityInfoFixture() {
        TestHost::getInstance().reset();
    }

    FSPAvailabilityInformation s1;
};

BOOST_FIXTURE_TEST_SUITE(FSPAvailabilityInfoTest, FSPAvailabilityInfoFixture)


/// SlownessInformation
BOOST_AUTO_TEST_CASE(FSPAvailabilityInfo_checkMsg) {
    FSPTaskList proxys(RandomQueueGenerator().createRandomQueue(1000.0));
    proxys.sortMinSlowness();
    s1.setAvailability(1024, 512, proxys, 1000.0);
    Logger::msg("Test.RI", INFO, s1);

    boost::shared_ptr<FSPAvailabilityInformation> p;
    CheckMsgMethod::check(s1, p);
}

BOOST_AUTO_TEST_SUITE_END()   // FSPAvailabilityInfoTest

} // namespace stars
