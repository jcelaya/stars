/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
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

#include "SimAppDatabase.hpp"
#include "Logger.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost::filesystem;


long int SimAppDatabase::lastInstance = 0, SimAppDatabase::lastRequest = 0;
unsigned long int SimAppDatabase::totalApps = 0, SimAppDatabase::totalAppsMemory = 0;
unsigned long int SimAppDatabase::totalInstances = 0, SimAppDatabase::totalInstancesMemory = 0;
unsigned long int SimAppDatabase::totalRequests = 0, SimAppDatabase::totalRequestsMemory = 0;


std::ostream & operator<<(std::ostream & os, const SimAppDatabase & s) {
    return os << s.apps.size() << " apps, " << s.instances.size() << " instances, " << s.requests.size() << " requests";
}


SimAppDatabase & SimAppDatabase::getCurrentDatabase() {
    SimAppDatabase & sdb = Simulator::getInstance().getCurrentNode().getDatabase();
    LogMsg("Database.Sim", DEBUG) << "Getting database from node " << Simulator::getInstance().getCurrentNode().getLocalAddress()
    << ": " << sdb;
    return sdb;
}


void SimAppDatabase::createAppDescription(const std::string & name, const TaskDescription & req) {
    lastApp.first = name;
    lastApp.second = req;
    apps[name] = req;
    totalApps++;
    totalAppsMemory += name.size() + sizeof(TaskDescription);
    LogMsg("Database.Sim", DEBUG) << "Created app " << name << ", resulting in " << *this;
}


void SimAppDatabase::dropAppDescription(const std::string & name) {
    totalApps--;
    totalAppsMemory -= name.size() + sizeof(TaskDescription);
    apps.erase(name);
    LogMsg("Database.Sim", DEBUG) << "Removed app " << name << ", resulting in " << *this;
}


void SimAppDatabase::appInstanceFinished(long int appId) {
    LogMsg("Database.Sim", DEBUG) << "Instance finished " << appId;
    for (map<long int, Request>::iterator it = requests.begin(); it != requests.end();) {
        LogMsg("Database.Sim", DEBUG) << "Checking request " << it->first << ": " << it->second;
        if (it->second.appId == appId) {
            totalRequests--;
            totalRequestsMemory -= sizeof(Request) + it->second.tasks.size() * sizeof(AppInstance::Task *);
            LogMsg("Database.Sim", DEBUG) << "This request belongs to intance " << appId << ", erasing.";
            requests.erase(it++);
        } else it++;
    }
    map<long int, AppInstance>::iterator inst = instances.find(appId);
    if (inst == instances.end()) {
        LogMsg("Database.Sim", ERROR) << "Error getting data for app " << appId;
        return;
    }
    totalInstances--;
    totalInstancesMemory -= sizeof(AppInstance) + inst->second.tasks.size() * sizeof(AppInstance::Task);
    instances.erase(inst);
    LogMsg("Database.Sim", DEBUG) << "Removed instance " << appId << ", resulting in " << *this;
}


bool SimAppDatabase::getNextAppRequest(long int appId, std::map<long int, Request>::const_iterator & it, bool valid) {
    if (!valid) it = requests.begin();
    else it++;
    while (it != requests.end() && it->second.appId != appId) it++;
    return it != requests.end();
}


TaskBagAppDatabase::TaskBagAppDatabase() : db(boost::filesystem::path(":memory:")) {
}


void TaskBagAppDatabase::createApp(const std::string & name, const TaskDescription & req) {
    SimAppDatabase::getCurrentDatabase().createAppDescription(name, req);
}


void TaskBagAppDatabase::getAppRequirements(long int appId, TaskDescription & req) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) throw Database::Exception(db) << "Error getting data for app " << appId;
    req = it->second.req;
}


long int TaskBagAppDatabase::createAppInstance(const std::string & name, Time deadline) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();

    map<string, TaskDescription>::iterator it = sdb.apps.find(name);
    if (it == sdb.apps.end()) throw Database::Exception(db) << "No application with name " << name;

    // Create instance
    sdb.lastInstance++;
    LogMsg("Database.Sim", DEBUG) << "Creating instance " << sdb.lastInstance << " for application " << name;
    SimAppDatabase::AppInstance & inst = sdb.instances[sdb.lastInstance];
    inst.req = it->second;
    inst.req.setDeadline(deadline);
    inst.ctime = Time::getCurrentTime();

    // Create tasks
    inst.tasks.resize(inst.req.getNumTasks());

    SimAppDatabase::totalInstances++;
    SimAppDatabase::totalInstancesMemory += sizeof(SimAppDatabase::AppInstance)
                                            + inst.tasks.size() * sizeof(SimAppDatabase::AppInstance::Task);
    LogMsg("Database.Sim", DEBUG) << "Created instance " << sdb.lastInstance << ", resulting in " << sdb;

    return sdb.lastInstance;
}


void TaskBagAppDatabase::requestFromReadyTasks(long int appId, TaskBagMsg & msg) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) {
        LogMsg("Database.Sim", ERROR) << "Error getting data for app " << appId;
        return;
    }
    sdb.lastRequest++;
    SimAppDatabase::Request & r = sdb.requests[sdb.lastRequest];
    r.appId = appId;
    r.acceptedTasks = r.numNodes = 0;

    // Look for ready tasks
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::READY)
            r.tasks.push_back(&(*t));
    }
    LogMsg("Database.Sim", DEBUG) << "Created request " << sdb.lastRequest << ": " << r;

    SimAppDatabase::totalRequests++;
    SimAppDatabase::totalRequestsMemory +=
        sizeof(SimAppDatabase::Request) + r.tasks.size() * sizeof(SimAppDatabase::AppInstance::Task *);
    LogMsg("Database.Sim", DEBUG) << "Database is now " << sdb;

    msg.setRequestId(sdb.lastRequest);
    msg.setFirstTask(1);
    msg.setLastTask(r.tasks.size());
    msg.setMinRequirements(it->second.req);
}


long int TaskBagAppDatabase::getInstanceId(long int rid) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    long int appId = sdb.getAppId(rid);
    if (appId == -1) throw Database::Exception(db) << "No request with id " << rid;
    return appId;
}


void TaskBagAppDatabase::startSearch(long int rid, Time timeout) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end()) throw Database::Exception(db) << "No request with id " << rid;
    it->second.rtime = it->second.stime = Time::getCurrentTime();
    LogMsg("Database.Sim", DEBUG) << "Submitting request " << rid << ": " << it->second;
    if (sdb.instances.count(it->second.appId) == 0) throw Database::Exception(db) << "No instance with id" << it->second.appId;
    sdb.instances[it->second.appId].rtime = it->second.rtime;
    for (vector<SimAppDatabase::AppInstance::Task *>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++)
        if (*t != NULL)
            (*t)->state = SimAppDatabase::AppInstance::Task::SEARCHING;
}


unsigned int TaskBagAppDatabase::cancelSearch(long int rid) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end()) throw Database::Exception(db) << "No request with id " << rid;
    unsigned int readyTasks = 0;
    for (vector<SimAppDatabase::AppInstance::Task *>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++)
        if (*t != NULL && (*t)->state == SimAppDatabase::AppInstance::Task::SEARCHING) {
            // If any task was still in searching state, search stops here
            it->second.stime = Time::getCurrentTime();
            (*t)->state = SimAppDatabase::AppInstance::Task::READY;
            readyTasks++;
            *t = NULL;
        }
    LogMsg("Database.Sim", DEBUG) << "Canceled " << readyTasks << " tasks from request " << rid << ": " << it->second;
    return readyTasks;
}


void TaskBagAppDatabase::acceptedTasks(const CommAddress & src, long int rid, unsigned int firstRtid, unsigned int lastRtid) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end()) throw Database::Exception(db) << "No request with id " << rid;
    LogMsg("Database.Sim", DEBUG) << src << " accepts " << (lastRtid - firstRtid + 1) << " tasks from request " << rid << ": " << it->second;
    // Update last search time
    it->second.stime = Time::getCurrentTime();
    it->second.numNodes++;
    // Check all tasks are in this request
    for (unsigned int t = firstRtid - 1; t < lastRtid; t++) {
        if (t >= it->second.tasks.size() || it->second.tasks[t] == NULL) {
            LogMsg("Database.Sim", ERROR) << "No task " << t << " in request with id " << rid;
            continue;
        }
        it->second.acceptedTasks++;
        it->second.tasks[t]->state = SimAppDatabase::AppInstance::Task::EXECUTING;
        it->second.tasks[t]->atime = Time::getCurrentTime();
        it->second.tasks[t]->host = src;
    }
    LogMsg("Database.Sim", DEBUG) << "Done: " << it->second;
}


bool TaskBagAppDatabase::taskInRequest(unsigned int tid, long int rid) {
    tid--;
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    LogMsg("Database.Sim", DEBUG) << "Checking if task " << tid << " is in request " << rid;
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end())
        LogMsg("Database.Sim", DEBUG) << "Request " << rid << " does not exist";
    else
        LogMsg("Database.Sim", DEBUG) << "Request " << rid << " is " << it->second;
    return it != sdb.requests.end() && it->second.tasks.size() > tid && it->second.tasks[tid] != NULL;
}


unsigned int TaskBagAppDatabase::getNumTasksInNode(const CommAddress & node) {
    return 0;
}


void TaskBagAppDatabase::getAppsInNode(const CommAddress & node, std::list<long int> & apps) {

}


bool TaskBagAppDatabase::finishedTask(const CommAddress & src, long int rid, unsigned int rtid) {
    rtid--;
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end()) throw Database::Exception(db) << "No request with id " << rid;
    LogMsg("Database.Sim", DEBUG) << "Finished task " << rtid << " from request " << it->first << ": " << it->second;
    if (it->second.tasks.size() <= rtid)
        throw Database::Exception(db) << "No task " << rtid << " in request with id " << rid;
    else if (it->second.tasks[rtid] == NULL) {
        LogMsg("Database.Sim", WARN) << "Task " << rtid << " of request " << rid << " already finished";
        return false;
    } else {
        it->second.tasks[rtid]->state = SimAppDatabase::AppInstance::Task::FINISHED;
        it->second.tasks[rtid]->ftime = Time::getCurrentTime();
        it->second.tasks[rtid] = NULL;
        return true;
    }
}


bool TaskBagAppDatabase::abortedTask(const CommAddress & src, long int rid, unsigned int rtid) {
    rtid--;
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end()) throw Database::Exception(db) << "No request with id " << rid;
    LogMsg("Database.Sim", DEBUG) << src << " aborts task " << rtid << " from request " << rid << ": " << it->second;
    if (it->second.tasks.size() <= rtid || it->second.tasks[rtid] == NULL) {
        return false;
    }
    it->second.tasks[rtid]->state = SimAppDatabase::AppInstance::Task::READY;
    it->second.tasks[rtid] = NULL;
    return true;
}


void TaskBagAppDatabase::deadNode(const CommAddress & fail) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    // Make a list of all tasks that where executing in that node
    LogMsg("Database.Sim", DEBUG) << "Node " << fail << " fails, looking for its tasks:";
    for (map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.begin(); it != sdb.requests.end(); it++) {
        LogMsg("Database.Sim", DEBUG) << "Checking request " << it->first << ": " << it->second;
        for (vector<SimAppDatabase::AppInstance::Task *>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
            if (*t != NULL && (*t)->state == SimAppDatabase::AppInstance::Task::EXECUTING && (*t)->host == fail) {
                // Change their status to READY
                (*t)->state = SimAppDatabase::AppInstance::Task::READY;
                // Take it out of its request
                *t = NULL;
            }
        }
    }
}


unsigned long int TaskBagAppDatabase::getNumFinished(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) throw Database::Exception(db) << "No instance with id " << appId;
    unsigned long int result = 0;
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::FINISHED) result++;
    }
    return result;
}


unsigned long int TaskBagAppDatabase::getNumReady(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) throw Database::Exception(db) << "No instance with id " << appId;
    unsigned long int result = 0;
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::READY) result++;
    }
    return result;
}


unsigned long int TaskBagAppDatabase::getNumExecuting(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) throw Database::Exception(db) << "No instance with id " << appId;
    unsigned long int result = 0;
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::EXECUTING) result++;
    }
    return result;
}


unsigned long int TaskBagAppDatabase::getNumInProcess(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) throw Database::Exception(db) << "No instance with id " << appId;
    unsigned long int result = 0;
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::EXECUTING || t->state == SimAppDatabase::AppInstance::Task::SEARCHING) result++;
    }
    return result;
}


bool TaskBagAppDatabase::isFinished(long int appId) {
    return false;
}

Time TaskBagAppDatabase::getReleaseTime(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) throw Database::Exception(db) << "No instance with id " << appId;
    return it->second.rtime;
}