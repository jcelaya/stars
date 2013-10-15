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
#include "TestHost.hpp"
#include "CommLayer.hpp"
#include "Scheduler.hpp"
//#include "IBPScheduler.hpp"
//#include "MMPScheduler.hpp"
#include "DPScheduler.hpp"
#include "FSPScheduler.hpp"
#include "TaskBagMsg.hpp"
#include "DescriptionFile.hpp"
#include "StructureNode.hpp"
#include "ResourceNode.hpp"
#include "TaskStateChgMsg.hpp"
using namespace std;
//using namespace boost;
using namespace boost::posix_time;

/// Test objects
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(Sch)


/// Description file
BOOST_AUTO_TEST_CASE(testDescriptionFile) {
    TestHost::getInstance().reset();

    // ctor
    DescriptionFile df("testTask");

    BOOST_CHECK(df.getExecutable() == "ls -l > kk.txt");
    BOOST_CHECK(df.getResult() == "kk.txt");
    BOOST_CHECK(df.getLength() == "1000000000");
    BOOST_CHECK(df.getMemory() == "1000");
    BOOST_CHECK(df.getDisk() == "10000");
}


/*
 * /// Simple Scheduler
 * BOOST_AUTO_TEST_CASE(testSimple) {
 *        CommAddress addr("127.0.0.0");
 *        TestCommLayer comm(addr);
 *        StructureNode s(comm, 4);
 *        ExecutionNode e(comm, s);
 *        shared_ptr<SimpleScheduler> sched(new SimpleScheduler(e));
 *        e.setScheduler(sched);
 *        list<shared_ptr<Task> > & tasks = sched->getTasks();
 *        shared_ptr<TaskStateChgMsg> msg(new TaskStateChgMsg);
 *        msg->setOldState(Task::Running);
 *        msg->setNewState(Task::Finished);
 *
 *        // Check creation
 *        const LinearAvailabilityFunction & avail = *sched->getAvailability();
 *        testCat.debugStream() << "New availability: " << avail;
 *        {
 *                ptime time1 = Time::getCurrentTime();
 *                unsigned long int a = avail(Time::getCurrentTime() + seconds(1));
 *                ptime time2 = Time::getCurrentTime();
 *                BOOST_CHECK(a <= 1000);
 *                BOOST_CHECK(a >= 1000 - (unsigned long int)ceil(1000.0 * (time2 - time1).total_seconds()));
 }

 // Add a pair of tasks
 shared_ptr<TaskDescription> task1desc(new TaskDescription), task2desc(new TaskDescription);
 shared_ptr<TaskBagMsg> task1req(new TaskBagMsg), task2req(new TaskBagMsg);
 task1req->setRequester(CommAddress("127.0.0.1"));
 task1req->setRequestId(1);
 task1req->setMinRequirements(task1desc);
 task1req->setFirstTask(1);
 task1req->setLastTask(1);
 task1desc->length = 300000;
 task1desc->deadline = Time::getCurrentTime(); // Deadline should be ignored
 task2req->setRequester(CommAddress("127.0.0.1"));
 task2req->setRequestId(2);
 task2req->setMinRequirements(task2desc);
 task2req->setFirstTask(1);
 task2req->setLastTask(1);
 task2desc->length = 600000;
 task2desc->deadline = Time::getCurrentTime() + seconds(800);

 BOOST_CHECK(sched->accept(task1req));
 BOOST_CHECK(!sched->accept(task2req));
 // Only one task, it does not depend on the deadline
 task2desc->deadline = Time::getCurrentTime() + seconds(1000);
 BOOST_CHECK(!sched->accept(task2req));

 list<shared_ptr<Task> >::iterator it = tasks.begin();
 shared_ptr<Task> task1 = (*it);
 BOOST_REQUIRE(task1.get());
 BOOST_CHECK_EQUAL(task1->getClientRequestId(), 1);
 BOOST_CHECK(task1->getStatus() == Task::Running);
 it++;
 BOOST_CHECK(it == tasks.end());
 msg->setTaskId(task1->getTaskId());

 sched->handle(addr, msg);
 BOOST_CHECK(tasks.empty());
 }


 /// FCFS Scheduler
 BOOST_AUTO_TEST_CASE(testFCFS) {
     CommAddress addr("127.0.0.0");
     TestCommLayer comm(addr);
     StructureNode s(comm, 4);
     ExecutionNode e(comm, s);
     shared_ptr<FCFSScheduler> sched(new FCFSScheduler(e));
     e.setScheduler(sched);
     list<shared_ptr<Task> > & tasks = sched->getTasks();
     shared_ptr<TaskStateChgMsg> msg(new TaskStateChgMsg);
     msg->setOldState(Task::Running);
     msg->setNewState(Task::Finished);

     // Check creation
     const LinearAvailabilityFunction & avail = *sched->getAvailability();
     testCat.debugStream() << "New availability: " << avail;
     {
         ptime time1 = Time::getCurrentTime();
         unsigned long int a = avail(Time::getCurrentTime() + seconds(1));
         ptime time2 = Time::getCurrentTime();
         BOOST_CHECK(a <= 1000);
         BOOST_CHECK(a >= 1000 - (unsigned long int)ceil(1000.0 * (time2 - time1).total_seconds()));
 }

 // Add a pair of tasks
 shared_ptr<TaskDescription> task1desc(new TaskDescription), task2desc(new TaskDescription);
 shared_ptr<TaskBagMsg> task1req(new TaskBagMsg), task2req(new TaskBagMsg);
 task1req->setRequester(CommAddress("127.0.0.1"));
 task1req->setRequestId(1);
 task1req->setMinRequirements(task1desc);
 task1req->setFirstTask(1);
 task1req->setLastTask(1);
 task1desc->length = 300000;
 task1desc->deadline = Time::getCurrentTime() + seconds(600);
 task2req->setRequester(CommAddress("127.0.0.1"));
 task2req->setRequestId(2);
 task2req->setMinRequirements(task2desc);
 task2req->setFirstTask(1);
 task2req->setLastTask(1);
 task2desc->length = 600000;
 task2desc->deadline = Time::getCurrentTime() + seconds(800);

 BOOST_CHECK(sched->accept(task1req));
 BOOST_CHECK(!sched->accept(task2req));
 task2desc->deadline = Time::getCurrentTime() + seconds(1000);
 BOOST_CHECK(sched->accept(task2req));

 {
     list<shared_ptr<Task> >::iterator it = tasks.begin();
     shared_ptr<Task> task1 = (*it);
     BOOST_REQUIRE(task1.get());
     BOOST_CHECK_EQUAL(task1->getClientRequestId(), 1);
     BOOST_CHECK(task1->getStatus() == Task::Running);
     it++;
     shared_ptr<Task> task2 = (*it);
     BOOST_REQUIRE(task2.get());
     BOOST_CHECK_EQUAL(task2->getClientRequestId(), 2);
     BOOST_CHECK(task2->getStatus() == Task::Prepared);
     it++;
     BOOST_CHECK(it == tasks.end());
     msg->setTaskId(task1->getTaskId());
 }

 sched->handle(addr, msg);
 {
     list<shared_ptr<Task> >::iterator it = tasks.begin();
     shared_ptr<Task> task2 = (*it);
     BOOST_REQUIRE(task2.get());
     BOOST_CHECK_EQUAL(task2->getClientRequestId(), 2);
     BOOST_CHECK(task2->getStatus() == Task::Running);
     it++;
     BOOST_CHECK(it == tasks.end());
 }
 }
 */


/// EDF Scheduler
BOOST_AUTO_TEST_CASE(testEDF) {
    TestHost::getInstance().reset();
    Time reference = Time::getCurrentTime();

    CommAddress addr = CommLayer::getInstance().getLocalAddress();
    ResourceNode * rn = new ResourceNode;
    CommLayer::getInstance().registerService(rn);
    DPScheduler * sched = new DPScheduler(*rn);
    CommLayer::getInstance().registerService(sched);
    list<boost::shared_ptr<Task> > & tasks = sched->getTasks();
    TaskStateChgMsg msg;
    msg.setOldState(Task::Running);
    msg.setNewState(Task::Finished);

    // Check creation
    const AvailabilityInformation & avail = sched->getAvailability();
    Logger::msg("Test.Sch", DEBUG, "New availability: ", avail);
    {
        Time time1 = reference;
        unsigned long int a = sched->getAvailabilityBefore(reference + Duration(1.0));
        Time time2 = reference;
        BOOST_CHECK(a <= 1000);
        BOOST_CHECK(a >= 1000 - (unsigned long int)ceil(1000.0 * (time2 - time1).seconds()));
    }

    // Add three of tasks
    TaskDescription task1desc, task2desc, task3desc;
    TaskBagMsg task1req, task2req, task3req;
    task1desc.setLength(400000);
    task1desc.setDeadline(reference + Duration(1300.0));
    task1req.setRequester(addr);
    task1req.setRequestId(1);
    task1req.setMinRequirements(task1desc);
    task1req.setFirstTask(1);
    task1req.setLastTask(1);
    task2desc.setLength(200000);
    task2desc.setDeadline(reference + Duration(400.0));
    task2req.setRequester(addr);
    task2req.setRequestId(2);
    task2req.setMinRequirements(task2desc);
    task2req.setFirstTask(1);
    task2req.setLastTask(1);
    task3desc.setLength(900000);
    task3desc.setDeadline(reference + Duration(1000.0));
    task3req.setRequester(addr);
    task3req.setRequestId(3);
    task3req.setMinRequirements(task3desc);
    task3req.setFirstTask(1);
    task3req.setLastTask(1);

    BOOST_CHECK(sched->accept(task2req));
    BOOST_CHECK(sched->accept(task1req));
    BOOST_CHECK(!sched->accept(task3req));
    task3desc.setLength(300000);
    task3req.setMinRequirements(task3desc);
    BOOST_CHECK(sched->accept(task3req));

    {
        list<boost::shared_ptr<Task> >::iterator it = tasks.begin();
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 2);
        BOOST_CHECK((*it)->getStatus() == Task::Running);
        msg.setTaskId((*it)->getTaskId());
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 3);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 1);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK(it == tasks.end());
    }

    sched->receiveMessage(addr, msg);
    {
        list<boost::shared_ptr<Task> >::iterator it = tasks.begin();
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 3);
        BOOST_CHECK((*it)->getStatus() == Task::Running);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 1);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK(it == tasks.end());
    }

    BOOST_CHECK(!sched->accept(task2req));
    task2desc.setLength(50000);
    task2req.setMinRequirements(task2desc);
    BOOST_CHECK(sched->accept(task2req));
    {
        list<boost::shared_ptr<Task> >::iterator it = tasks.begin();
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 3);
        BOOST_CHECK((*it)->getStatus() == Task::Running);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 2);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 1);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK(it == tasks.end());
    }
}


/// MinSlowness Scheduler
BOOST_AUTO_TEST_CASE(testMinSlowness) {
    TestHost::getInstance().reset();

    CommAddress addr = CommLayer::getInstance().getLocalAddress();
    ResourceNode * rn = new ResourceNode();
    CommLayer::getInstance().registerService(rn);
    stars::FSPScheduler * sched = new stars::FSPScheduler(*rn);
    CommLayer::getInstance().registerService(sched);
    list<boost::shared_ptr<Task> > & tasks = sched->getTasks();
    TaskStateChgMsg msg;
    msg.setOldState(Task::Running);
    msg.setNewState(Task::Finished);

    // Check creation
    BOOST_CHECK_EQUAL(sched->getAvailability().getMinimumSlowness(), 0.0);

    // Add three of tasks
    TaskDescription task1desc, task2desc, task3desc;
    TaskBagMsg task1req, task2req, task3req;
    task1desc.setLength(400000);
    task1desc.setNumTasks(5);
    task1req.setRequester(addr);
    task1req.setRequestId(1);
    task1req.setMinRequirements(task1desc);
    task1req.setFirstTask(1);
    task1req.setLastTask(1);
    task2desc.setLength(200000);
    task2desc.setNumTasks(5);
    task2req.setRequester(addr);
    task2req.setRequestId(2);
    task2req.setMinRequirements(task2desc);
    task2req.setFirstTask(1);
    task2req.setLastTask(1);
    task3desc.setLength(900000);
    task3desc.setNumTasks(5);
    task3req.setRequester(addr);
    task3req.setRequestId(3);
    task3req.setMinRequirements(task3desc);
    task3req.setFirstTask(1);
    task3req.setLastTask(1);

    BOOST_CHECK(sched->accept(task3req));
    BOOST_CHECK(sched->accept(task1req));
    BOOST_CHECK(sched->accept(task2req));

    {
        list<boost::shared_ptr<Task> >::iterator it = tasks.begin();
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 3);
        BOOST_CHECK((*it)->getStatus() == Task::Running);
        msg.setTaskId((*it)->getTaskId());
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 2);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 1);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK(it == tasks.end());
        BOOST_CHECK_CLOSE(sched->getAvailability().getMinimumSlowness(), 0.0055, 0.01);
    }

    sched->receiveMessage(addr, msg);
    {
        list<boost::shared_ptr<Task> >::iterator it = tasks.begin();
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 2);
        BOOST_CHECK((*it)->getStatus() == Task::Running);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 1);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK(it == tasks.end());
        BOOST_CHECK_CLOSE(sched->getAvailability().getMinimumSlowness(), 0.0015, 0.01);
    }

    task3desc.setLength(50000);
    task3req.setMinRequirements(task3desc);
    BOOST_CHECK(sched->accept(task3req));
    {
        list<boost::shared_ptr<Task> >::iterator it = tasks.begin();
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 2);
        BOOST_CHECK((*it)->getStatus() == Task::Running);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 3);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK_EQUAL((*it)->getClientRequestId(), 1);
        BOOST_CHECK((*it)->getStatus() == Task::Prepared);
        it++;
        BOOST_CHECK(it == tasks.end());
        BOOST_CHECK_CLOSE(sched->getAvailability().getMinimumSlowness(), 0.005, 0.01);
    }
}

BOOST_AUTO_TEST_SUITE_END()   // Sch

BOOST_AUTO_TEST_SUITE_END()   // Cor
