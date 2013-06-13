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

#include "CheckMsg.hpp"
#include "TaskEventMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "TaskBagMsg.hpp"
#include "Task.hpp"
#include <stdexcept>
using namespace std;
//using namespace boost;

/// Test cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(ExecMsg)

/// TaskEventMsg
BOOST_AUTO_TEST_CASE(testTaskEventMsg) {
    // Ctor
    TaskEventMsg e;
    boost::shared_ptr<TaskEventMsg> p;

    // setTaskId
    e.setTaskId(2456);

    CheckMsgMethod::check(e, p);
    BOOST_CHECK(p->getTaskId() == 2456);
}

/// TaskStateChgMsg
BOOST_AUTO_TEST_CASE(testTaskStateChgMsg) {
    // Ctor
    TaskStateChgMsg e;
    boost::shared_ptr<TaskStateChgMsg> p;
    e.setTaskId(0);

    // setOldState
    e.setOldState(Task::Prepared);

    // setNewState
    e.setNewState(Task::Running);

    CheckMsgMethod::check(e, p);
    BOOST_CHECK(p->getOldState() == Task::Prepared);
    BOOST_CHECK(p->getNewState() == Task::Running);
}

/// TaskBagMsg
BOOST_AUTO_TEST_CASE(testTaskBagMsg) {
    // Ctor
    TaskBagMsg e;
    boost::shared_ptr<TaskBagMsg> p;

    // setForEN
    e.setForEN(true);

    // isForEN
    BOOST_CHECK(e.isForEN());

    // setFromEN
    e.setFromEN(true);

    // isFromEN
    BOOST_CHECK(e.isFromEN());

    // setFirstTask
    e.setFirstTask(4);

    // getFirstTask
    BOOST_CHECK(e.getFirstTask() == 4);

    // setLastTask
    e.setLastTask(20);

    // getLastTask
    BOOST_CHECK(e.getLastTask() == 20);

    // setRequester
    CommAddress o("127.0.0.1", 2030);
    e.setRequester(o);

    // getRequester
    BOOST_CHECK(e.getRequester() == o);

    // setRequestId
    e.setRequestId(100);

    // getRequestId
    BOOST_CHECK(e.getRequestId() == 100);

    // setMinRequirements
    TaskDescription td;
    td.setLength(3000);
    td.setDeadline(Time(12342356));
    e.setMinRequirements(td);

    // getMinRequirements
    BOOST_CHECK(e.getMinRequirements().getLength() == 3000);

    CheckMsgMethod::check(e, p);
    BOOST_CHECK(p->getMinRequirements().getLength() == 3000);
    BOOST_CHECK(p->getFirstTask() == 4);
    BOOST_CHECK(p->getLastTask() == 20);
    BOOST_CHECK(p->isForEN());
    BOOST_CHECK(p->isFromEN());
}

BOOST_AUTO_TEST_SUITE_END()   // ExecMsg

BOOST_AUTO_TEST_SUITE_END()   // Cor
