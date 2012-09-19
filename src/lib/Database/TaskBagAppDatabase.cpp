/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya, María Ángeles Giménez
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

#include "Logger.hpp"
#include "TaskBagAppDatabase.hpp"
using namespace std;
using namespace boost::filesystem;


TaskBagAppDatabase::TaskBagAppDatabase() {
    db.open(ConfigurationManager::getInstance().getDatabasePath());
    createTables();
}


void TaskBagAppDatabase::createTables() {
    // Data model
    db.execute("create table if not exists tb_app_description (\
   name text primary key,\
   num_tasks integer,\
   length integer,\
   memory integer,\
   disk integer,\
   input integer,\
   output integer)"
              );
    db.execute("create table if not exists tb_app_instance (\
   id integer primary key,\
   app_type text not null references tb_app_description(name) on delete cascade on update cascade,\
   ctime integer not null,\
   rtime integer,\
   deadline integer)"
              );
    db.execute("create table if not exists tb_task (\
   tid integer not null,\
   app_instance integer not null references tb_app_instance(id) on delete cascade,\
   state text not null default 'READY',\
   atime integer,\
   ftime integer,\
   host_IP text,\
   host_port integer,\
   primary key (tid, app_instance))"
              );
    db.execute("create table if not exists tb_request (\
   rid integer primary key autoincrement,\
   app_instance integer not null references tb_app_instance(id) on delete cascade,\
   timeout integer)"
              );
    // There is a check that cannot be done here: tid must be a task from the same instance as the request rid
    db.execute("create table if not exists tb_task_request (\
   rid integer not null references tb_request(rid) on delete cascade,\
   rtid integer not null,\
   tid integer not null,\
   primary key (rid, rtid))"
              );
}


bool TaskBagAppDatabase::createApp(const std::string & name, const TaskDescription & req) {
    return Database::Query(db, "insert into tb_app_description values (?, ?, ?, ?, ?, ?, ?)")
    .par(name).par(req.getNumTasks()).par(req.getLength()).par(req.getMaxMemory())
    .par(req.getMaxDisk()).par(req.getInputSize()).par(req.getOutputSize()).execute();
}


long int TaskBagAppDatabase::createAppInstance(const std::string & name, Time deadline) {
    long int instanceId = 0;
    db.beginTransaction();
    unsigned int numTasks;
    {
        Database::Query getNumTasks(db, "select num_tasks from tb_app_description where name = ?");
        if (getNumTasks.par(name).fetchNextRow()) {
            numTasks = getNumTasks.getInt();

            // Create instance
            if (Database::Query(db, "insert into tb_app_instance (app_type, ctime, deadline) values (?, ?, ?)")
                    .par(name).par(Time::getCurrentTime().getRawDate()).par(deadline.getRawDate()).execute()) {
                instanceId = db.getLastRowid();

                // Create tasks
                bool good = true;
                Database::Query createTask(db, "insert into tb_task (tid, app_instance) values (?, ?)");
                for (unsigned int t = 1; good && t <= numTasks; t++) {
                    good &= createTask.par(t).par(instanceId).execute();
                }

                if (good) {
                    db.commitTransaction();
                    return instanceId;
                }
            }
        }
    }
    LogMsg("Database.App", WARN) << "No instance created for application " << name;
    db.rollbackTransaction();
    return -1;
}


void TaskBagAppDatabase::requestFromReadyTasks(long int appId, TaskBagMsg & msg) {
    long int requestId = 0;
    unsigned int numTasks = 0;
    TaskDescription req;
    msg.setFirstTask(1);

    db.beginTransaction(); {
        // Get app requirements if they exist
        Database::Query selectQuery(db, "select num_tasks, length, memory, disk, input, output from tb_app_description where name in "
                                    "(select app_type from tb_app_instance where id = ?)");
        if (selectQuery.par(appId).fetchNextRow()) {
            req.setNumTasks(selectQuery.getInt());
            req.setLength(selectQuery.getInt());
            req.setMaxMemory(selectQuery.getInt());
            req.setMaxDisk(selectQuery.getInt());
            req.setInputSize(selectQuery.getInt());
            req.setOutputSize(selectQuery.getInt());

            Database::Query getDeadline(db, "select deadline from tb_app_instance where id = ?");
            if (getDeadline.par(appId).fetchNextRow()) {
                req.setDeadline(Time(getDeadline.getInt()));

                // Look for ready tasks
                Database::Query getReady(db, "select tid from tb_task where app_instance = ? and state = 'READY'");
                getReady.par(appId);
                if (getReady.fetchNextRow()) {
                    // Create request
                    if (Database::Query(db, "insert into tb_request (app_instance) values (?)").par(appId).execute()) {
                        requestId = db.getLastRowid();

                        // Associate task and request ids
                        Database::Query associateTidRid(db, "insert into tb_task_request values (?, ?, ?)");
                        bool good = associateTidRid.par(requestId).par(++numTasks).par(getReady.getInt()).execute();
                        while (good && getReady.fetchNextRow()) {
                            good &= associateTidRid.par(requestId).par(++numTasks).par(getReady.getInt()).execute();
                        }

                        if (good) {
                            db.commitTransaction();
                            msg.setRequestId(requestId);
                            msg.setLastTask(numTasks);
                            msg.setMinRequirements(req);
                            return;
                        }
                    }
                }
            }
        }
    }
    db.rollbackTransaction();
    msg.setLastTask(0);
}


long int TaskBagAppDatabase::getInstanceId(long int rid) {
    Database::Query getId(db, "select app_instance from tb_request where rid = ?");

    if (getId.par(rid).fetchNextRow())
        return getId.getInt();
    else {
        LogMsg("Database.App", WARN) << "No request with id " << rid;
        return -1;
    }
}


bool TaskBagAppDatabase::startSearch(long int rid, Time timeout) {
    db.beginTransaction();
    if(Database::Query(db, "update tb_app_instance set rtime = ? "
       "where rtime is NULL and id in (select app_instance from tb_request where rid = ?)")
       .par(Time::getCurrentTime().getRawDate()).par(rid).execute()
       && Database::Query(db, "update tb_task set state = 'SEARCHING' where "
          "app_instance = (select app_instance from tb_request where rid = ?) "
          "and tid in (select tid from tb_task_request where rid = ?1)")
          .par(rid).execute()
          && Database::Query(db, "update tb_request set timeout = ? where rid = ?")
             .par(timeout.getRawDate()).par(rid).execute()) {
        db.commitTransaction();
        return true;
    } else {
        db.rollbackTransaction();
        return false;
    }
}


unsigned int TaskBagAppDatabase::cancelSearch(long int rid) {
    db.beginTransaction();
    if (Database::Query(db, "update tb_task set state = 'READY' where "
                        "app_instance = (select app_instance from tb_request where rid = ?) "
                        "and tid in (select tid from tb_task_request where rid = ?1) and state = 'SEARCHING'")
        .par(rid).execute() ) {
        unsigned int readyTasks = db.getChangedRows();
        if (Database::Query(db, "delete from tb_task_request where rid = ? and tid in "
                            "(select tid from tb_task where state = 'READY' and "
                            "app_instance = (select app_instance from tb_request where rid = ?1))")
            .par(rid).execute()) {
            db.commitTransaction();
            return readyTasks;
        }
    }
    db.rollbackTransaction();
    return 0;
}


void TaskBagAppDatabase::acceptedTasks(const CommAddress & src, long int rid, unsigned int firstRtid, unsigned int lastRtid) {
    Database::Query(db, "update tb_task set state = 'EXECUTING', atime = ?, host_IP = ?, host_port = ? where tid in "
                    "(select tid from tb_task_request where rid = ? and rtid between ? and ?) and "
                    "app_instance = (select app_instance from tb_request where rid = ?4)")
    .par(Time::getCurrentTime().getRawDate()).par(src.getIPString())
    .par(src.getPort()).par(rid).par(firstRtid).par(lastRtid).execute();
}


bool TaskBagAppDatabase::taskInRequest(unsigned int tid, long int rid) {
    return Database::Query(db, "select * from tb_task_request where rid = ? and rtid = ?").par(rid).par(tid).fetchNextRow();
}


unsigned int TaskBagAppDatabase::getNumTasksInNode(const CommAddress & node) {
    unsigned int result = 0;
    Database::Query getNumTasks(db, "select count(*) from tb_task where state = 'EXECUTING' and host_IP = ? and host_port = ?");
    return result;
}


void TaskBagAppDatabase::getAppsInNode(const CommAddress & node, std::list<long int> & apps) {

}


bool TaskBagAppDatabase::finishedTask(const CommAddress & src, long int rid, unsigned int rtid) {
    if (Database::Query(db, "select * from tb_task where state = 'FINISHED' and "
                        "tid = (select tid from tb_task_request where rid = ? and rtid = ?) and "
                        "app_instance = (select app_instance from tb_request where rid = ?1)").par(rid).par(rtid).fetchNextRow()) {
        Database::Query getTid(db, "select tid from tb_task_request where rid = ? and rtid = ?");
        getTid.par(rid).par(rtid).fetchNextRow();
        LogMsg("Database.App", WARN) << "Task " << getTid.getInt() << " already finished in app instance " << getInstanceId(rid);
        return false;
    }
    Database::Query(db, "update tb_task set state = 'FINISHED', ftime = ? where host_IP = ? and host_port = ? and "
                    "tid = (select tid from tb_task_request where rid = ? and rtid = ?) and "
                    "app_instance = (select app_instance from tb_request where rid = ?4)")
    .par(Time::getCurrentTime().getRawDate()).par(src.getIPString()).par(src.getPort())
    .par(rid).par(rtid).execute();
    return true;
}


bool TaskBagAppDatabase::abortedTask(const CommAddress & src, long int rid, unsigned int rtid) {
    if (!Database::Query(db, "select * from tb_task where state = 'EXECUTING' and host_IP = ? and host_port = ? and "
                         "tid = (select tid from tb_task_request where rid = ? and rtid = ?) and "
                         "app_instance = (select app_instance from tb_request where rid = ?1)")
            .par(src.getIPString()).par(src.getPort()).par(rid).par(rtid).fetchNextRow())
        return false;
    db.beginTransaction();
    if (// Change their status to READY
        Database::Query(db, "update tb_task set state = 'READY', atime = NULL, ftime = NULL, host_IP = NULL, host_port = NULL "
                        "where host_IP = ? and host_port = ? and "
                        "tid = (select tid from tb_task_request where rid = ? and rtid = ?) and "
                        "app_instance = (select app_instance from tb_request where rid = ?3)")
        .par(src.getIPString()).par(src.getPort()).par(rid).par(rtid).execute() &&
        // Take that task from its request
        Database::Query(db, "delete from tb_task_request where rid = ? and rtid = ?")
        .par(rid).par(rtid).execute())
        db.commitTransaction();
    else
        db.rollbackTransaction();
    return true;
}


void TaskBagAppDatabase::deadNode(const CommAddress & fail) {
    // Make a list of all tasks that where executing in that node
    Database::Query failedTasks(db, "select B.rid, A.tid from tb_task A, tb_request B where "
                                "state = 'EXECUTING' and host_IP = ? and host_port = ? and A.app_instance = B.app_instance");
    failedTasks.par(fail.getIPString()).par(fail.getPort());
    while (failedTasks.fetchNextRow()) {
        // Take it out of its request
        long int rid = failedTasks.getInt();
        long int tid = failedTasks.getInt();
        Database::Query(db, "delete from tb_task_request where rid = ? and tid = ?")
        .par(rid).par(tid).execute();
    }
    // Change their status to READY
    Database::Query(db, "update tb_task set state = 'READY', atime = NULL, ftime = NULL, host_IP = NULL, host_port = NULL "
                    "where host_IP = ? and host_port = ? and state = 'EXECUTING'").par(fail.getIPString()).par(fail.getPort()).execute();
}


unsigned long int TaskBagAppDatabase::getNumFinished(long int appId) {
    Database::Query count(db, "select count(*) from tb_task where app_instance = ? and state = 'FINISHED'");
    count.par(appId).fetchNextRow();
    return count.getInt();
}


unsigned long int TaskBagAppDatabase::getNumReady(long int appId) {
    Database::Query count(db, "select count(*) from tb_task where app_instance = ? and state = 'READY'");
    count.par(appId).fetchNextRow();
    return count.getInt();
}


unsigned long int TaskBagAppDatabase::getNumExecuting(long int appId) {
    Database::Query count(db, "select count(*) from tb_task where app_instance = ? and state = 'EXECUTING'");
    count.par(appId).fetchNextRow();
    return count.getInt();
}


unsigned long int TaskBagAppDatabase::getNumInProcess(long int appId) {
    Database::Query count(db, "select count(*) from tb_task where app_instance = ? and (state = 'EXECUTING' or state = 'SEARCHING')");
    count.par(appId).fetchNextRow();
    return count.getInt();
}


bool TaskBagAppDatabase::isFinished(long int appId) {
    return !Database::Query(db, "select * from tb_task where app_instance = ? and state != 'FINISHED'").par(appId).fetchNextRow();
}

Time TaskBagAppDatabase::getReleaseTime(long int appId) {
    Database::Query rt(db, "select rtime from tb_app_instance where id = ?");
    rt.par(appId).fetchNextRow();
    return Time((long int)rt.getInt());
}
