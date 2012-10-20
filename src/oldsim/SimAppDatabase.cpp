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

#include "SimAppDatabase.hpp"
#include "Logger.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost::filesystem;


long int SimAppDatabase::lastInstance = 0, SimAppDatabase::lastRequest = 0;
unsigned long int SimAppDatabase::totalInstances = 0, SimAppDatabase::totalInstancesMemory = 0;
unsigned long int SimAppDatabase::totalRequests = 0, SimAppDatabase::totalRequestsMemory = 0;


std::ostream & operator<<(std::ostream & os, const SimAppDatabase & s) {
    return os << s.instances.size() << " instances, " << s.requests.size() << " requests";
}


SimAppDatabase & SimAppDatabase::getCurrentDatabase() {
    SimAppDatabase & sdb = Simulator::getInstance().getCurrentNode().getDatabase();
    LogMsg("Database.Sim", DEBUG) << "Getting database from node " << Simulator::getInstance().getCurrentNode().getLocalAddress()
            << ": " << sdb;
    return sdb;
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


TaskBagAppDatabase::TaskBagAppDatabase() {
    // Do nothing
}


bool TaskBagAppDatabase::createApp(const std::string & name, const TaskDescription & req) {
    return true;
}


long int TaskBagAppDatabase::createAppInstance(const std::string & name, Time deadline) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();

    // Create instance
    sdb.lastInstance++;
    LogMsg("Database.Sim", DEBUG) << "Creating instance " << sdb.lastInstance << " for application " << name;
    SimAppDatabase::AppInstance & inst = sdb.instances[sdb.lastInstance];
    inst.req = sdb.nextApp;
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
    return SimAppDatabase::getCurrentDatabase().getAppId(rid);
}


bool TaskBagAppDatabase::startSearch(long int rid, Time timeout) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end() || sdb.instances.count(it->second.appId) == 0) return false;
    it->second.rtime = it->second.stime = Time::getCurrentTime();
    LogMsg("Database.Sim", DEBUG) << "Submitting request " << rid << ": " << it->second;
    sdb.instances[it->second.appId].rtime = it->second.rtime;
    for (vector<SimAppDatabase::AppInstance::Task *>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++)
        if (*t != NULL)
            (*t)->state = SimAppDatabase::AppInstance::Task::SEARCHING;
    return true;
}


unsigned int TaskBagAppDatabase::cancelSearch(long int rid) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end()) return 0;
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
    if (it == sdb.requests.end()) return;
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
        //it->second.tasks[t]->atime = Time::getCurrentTime();
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
    if (it == sdb.requests.end()) return false;
    LogMsg("Database.Sim", DEBUG) << "Finished task " << rtid << " from request " << it->first << ": " << it->second;
    if (it->second.tasks.size() <= rtid) return false;
    else if (it->second.tasks[rtid] == NULL) {
        LogMsg("Database.Sim", WARN) << "Task " << rtid << " of request " << rid << " already finished";
        return false;
    } else {
        it->second.tasks[rtid]->state = SimAppDatabase::AppInstance::Task::FINISHED;
        //it->second.tasks[rtid]->ftime = Time::getCurrentTime();
        it->second.tasks[rtid] = NULL;
        return true;
    }
}


bool TaskBagAppDatabase::abortedTask(const CommAddress & src, long int rid, unsigned int rtid) {
    rtid--;
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::Request>::iterator it = sdb.requests.find(rid);
    if (it == sdb.requests.end()) return false;
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
    if (it == sdb.instances.end()) return 0;
    unsigned long int result = 0;
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::FINISHED) result++;
    }
    return result;
}


unsigned long int TaskBagAppDatabase::getNumReady(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) return 0;
    unsigned long int result = 0;
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::READY) result++;
    }
    return result;
}


unsigned long int TaskBagAppDatabase::getNumExecuting(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) return 0;
    unsigned long int result = 0;
    for (vector<SimAppDatabase::AppInstance::Task>::iterator t = it->second.tasks.begin(); t != it->second.tasks.end(); t++) {
        if (t->state == SimAppDatabase::AppInstance::Task::EXECUTING) result++;
    }
    return result;
}


unsigned long int TaskBagAppDatabase::getNumInProcess(long int appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    map<long int, SimAppDatabase::AppInstance>::iterator it = sdb.instances.find(appId);
    if (it == sdb.instances.end()) return 0;
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
    if (it == sdb.instances.end()) return Time();
    return it->second.rtime;
}
