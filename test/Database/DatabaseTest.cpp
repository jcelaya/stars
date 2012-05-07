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
#include "Database.hpp"
#include "TaskBagAppDatabase.hpp"
using namespace std;
using namespace boost;
using namespace boost::posix_time;

/// Test cases
BOOST_AUTO_TEST_SUITE(Cor)   // Correctness test suite

BOOST_AUTO_TEST_SUITE(Db)


/// Basic Database control
BOOST_AUTO_TEST_CASE(testDatabase) {
	// ctor
	Database db = Database(":memory:");
	db.execute("create table if not exists project (name text primary key)");

	// Some insertions
	for (int i = 1; i <= 10; i++) {
		ostringstream oss;
		oss << "project" << i;
		Database::Query(db, "insert into project values (?)").par(oss.str()).execute();
	}

	// Select all rows in project
	Database::Query allQuery(db, "select name from project");
	int i = 1;
	while (allQuery.fetchNextRow()) {
		ostringstream oss;
		oss << "project" << i++;
		string read = allQuery.getStr();
		BOOST_TEST_MESSAGE("Read value " << read);
		BOOST_CHECK(read == oss.str());
	}
	BOOST_CHECK(i == 11);
	allQuery.reset();

	// Check the transaction mechanism
	db.beginTransaction();
	for (int i = 11; i <= 20; i++) {
		ostringstream oss;
		oss << "project" << i;
		Database::Query(db, "insert into project values (?)").par(oss.str()).execute();
	}
	db.rollbackTransaction();
	// Check we still have 10 projects project1..project10
	i = 1;
	while (allQuery.fetchNextRow()) {
		ostringstream oss;
		oss << "project" << i++;
		string read = allQuery.getStr();
		BOOST_TEST_MESSAGE("Read value " << read);
		BOOST_CHECK(read == oss.str());
	}
	BOOST_CHECK(i == 11);
	allQuery.reset();

	db.beginTransaction();
	for (int i = 11; i <= 15; i++) {
		ostringstream oss;
		oss << "project" << i;
		Database::Query(db, "insert into project values (?)").par(oss.str()).execute();
	}
	allQuery.reset();
	for (int i = 16; i <= 20; i++) {
		ostringstream oss;
		oss << "project" << i;
		Database::Query(db, "insert into project values (?)").par(oss.str()).execute();
	}
	db.commitTransaction();
	// Check we have 20 projects now project1..project20
	i = 1;
	while (allQuery.fetchNextRow()) {
		ostringstream oss;
		oss << "project" << i++;
		string read = allQuery.getStr();
		BOOST_TEST_MESSAGE("Read value " << read);
		BOOST_CHECK(read == oss.str());
	}
	BOOST_CHECK(i == 21);
	allQuery.reset();

	// Remove data
	db.execute("drop table project");
}

BOOST_AUTO_TEST_CASE(testTaskBagAppDatabase) {
	// Create tables
	TaskBagAppDatabase tbad;
	// Clean tables in cascade
	tbad.getDatabase().execute("delete from tb_app_description");

	// Create app
	TaskDescription desc1;
	desc1.setLength(1000);
	desc1.setNumTasks(4);
	tbad.createApp("app1", desc1);
	// Check it was created
	BOOST_CHECK(Database::Query(tbad.getDatabase(), "select * from tb_app_description where name = 'app1' and length = 1000").fetchNextRow());
	// Check we cannot create another app with the same name
	BOOST_CHECK_THROW(tbad.createApp("app1", desc1), Database::Exception);

	// Create an app instance
	Time deadline = Time::getCurrentTime();
	long int appInst = tbad.createAppInstance("app1", deadline);
	// Check we cannot create an app instance for an inexistent app
	BOOST_CHECK_THROW(tbad.createAppInstance("app2", deadline), Database::Exception);
	// Check that the task description from appInst is the same as app1
	TaskDescription desc2;
	tbad.getAppRequirements(appInst, desc2);
	BOOST_CHECK(desc1.getLength() == desc2.getLength());
	BOOST_CHECK(desc1.getNumTasks() == desc2.getNumTasks());
	// Prepare a taskbagmsg for the four ready tasks
	TaskBagMsg tbm, tmp;
	tbad.requestFromReadyTasks(appInst, tbm);
	BOOST_CHECK(tbm.getFirstTask() == 1);
	BOOST_CHECK(tbm.getLastTask() == desc1.getNumTasks());
	BOOST_CHECK(tbm.getMinRequirements().getLength() == desc1.getLength());
	BOOST_CHECK(tbm.getMinRequirements().getNumTasks() == desc1.getNumTasks());
	BOOST_CHECK(tbm.getMinRequirements().getDeadline() == deadline);
	// Check that the request id is ok
	BOOST_CHECK(tbad.getInstanceId(tbm.getRequestId()) == appInst);
	// Check there is an error for another request id
	BOOST_CHECK_THROW(tbad.getInstanceId(234526), Database::Exception);
	// Start the search
	tbad.startSearch(tbm.getRequestId(), deadline);
	// Check that all tasks are now not ready
	tbad.requestFromReadyTasks(appInst, tmp);
	BOOST_CHECK(tmp.getLastTask() == 0);
	// Accept some tasks
	tbad.acceptedTasks(CommAddress(1, 2030), tbm.getRequestId(), 1, 2);
	// Cancel the rest
	tbad.cancelSearch(tbm.getRequestId());
	// Check that tasks 3 and 4 are not anymore in this request
	BOOST_CHECK(!tbad.taskInRequest(3, tbm.getRequestId()));
	BOOST_CHECK(!tbad.taskInRequest(4, tbm.getRequestId()));
	// Now create a new request
	tbad.requestFromReadyTasks(appInst, tbm);
	BOOST_CHECK(tbm.getFirstTask() == 1);
	BOOST_CHECK(tbm.getLastTask() == 2);
	BOOST_CHECK(tbm.getMinRequirements().getLength() == desc1.getLength());
	BOOST_CHECK(tbm.getMinRequirements().getNumTasks() == 4);
	BOOST_CHECK(tbm.getMinRequirements().getDeadline() == deadline);
	// Test cascade delete
	tbad.getDatabase().execute("delete from tb_app_description where name = 'app1'");
}

BOOST_AUTO_TEST_SUITE_END()   // Db

BOOST_AUTO_TEST_SUITE_END()   // Cor
