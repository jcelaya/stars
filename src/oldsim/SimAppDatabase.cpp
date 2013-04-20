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


unsigned long int SimAppDatabase::totalInstances = 0, SimAppDatabase::totalInstancesMemory = 0;
unsigned long int SimAppDatabase::totalRequests = 0, SimAppDatabase::totalRequestsMemory = 0;


SimAppDatabase & SimAppDatabase::getCurrentDatabase() {
    return Simulator::getInstance().getCurrentNode().getDatabase();
}


bool SimAppDatabase::findAppInstance(int64_t appId) {
    if (instanceCache.first != appId) {
        map<int64_t, AppInstance>::iterator it = instances.find(appId);
        if (it == instances.end()) {
            LogMsg("Database.Sim", ERROR) << "Error getting data for app " << appId;
            return false;
        }
        instanceCache = make_pair(appId, &it->second);
    }
    return true;
}


bool SimAppDatabase::findRequest(int64_t rid) {
    if (requestCache.first != rid) {
        if (findAppInstance(getAppId(rid))) {
            for (list<SimAppDatabase::Request>::iterator request = instanceCache.second->requests.begin();
                    request != instanceCache.second->requests.end(); ++request)
                if (request->rid == rid) {
                    requestCache = make_pair(rid, &*request);
                    return true;
                }
        }
        return false;
    }
    return true;
}


void SimAppDatabase::appInstanceFinished(int64_t appId) {
    LogMsg("Database.Sim", DEBUG) << "Instance finished " << appId;
    map<int64_t, AppInstance>::iterator inst = instances.find(appId);
    if (inst == instances.end()) {
        LogMsg("Database.Sim", ERROR) << "Error getting data for app " << appId;
        return;
    }
    for (list<Request>::iterator it = inst->second.requests.begin(); it != inst->second.requests.end(); ++it) {
//        LogMsg("Database.Sim", DEBUG) << "Checking request " << it->first << ": " << it->second;
        totalRequests--;
        totalRequestsMemory -= sizeof(Request) + it->tasks.size() * sizeof(RemoteTask *);
//            LogMsg("Database.Sim", DEBUG) << "This request belongs to intance " << appId << ", erasing.";
    }
    totalInstances--;
    totalInstancesMemory -= sizeof(AppInstance) + inst->second.tasks.size() * sizeof(RemoteTask);
    instances.erase(inst);
    instanceCache.first = requestCache.first = -1;
}


TaskBagAppDatabase::TaskBagAppDatabase() {
    // Do nothing
}


bool TaskBagAppDatabase::createApp(const std::string & name, const TaskDescription & req) {
    return true;
}


int64_t TaskBagAppDatabase::createAppInstance(const std::string & name, Time deadline) {
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

    sdb.instanceCache = std::make_pair(sdb.lastInstance, &inst);

    SimAppDatabase::totalInstances++;
    SimAppDatabase::totalInstancesMemory += sizeof(SimAppDatabase::AppInstance)
            + inst.tasks.size() * sizeof(SimAppDatabase::RemoteTask);

    return sdb.lastInstance;
}


void TaskBagAppDatabase::requestFromReadyTasks(int64_t appId, TaskBagMsg & msg) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    if (sdb.findAppInstance(appId)) {
        SimAppDatabase::AppInstance & instance = *sdb.instanceCache.second;
        int64_t rid = instance.requests.empty() ? appId << SimAppDatabase::bitsPerRequest : instance.requests.back().rid + 1;
        instance.requests.push_back(SimAppDatabase::Request());
        SimAppDatabase::Request & r = instance.requests.back();
        r.rid = rid;
        r.acceptedTasks = r.numNodes = 0;

        // Look for ready tasks
        for (vector<SimAppDatabase::RemoteTask>::iterator t = instance.tasks.begin(); t != instance.tasks.end(); t++) {
            if (t->state == SimAppDatabase::RemoteTask::READY)
                r.tasks.push_back(&(*t));
        }
        r.remainingTasks = r.tasks.size();
        LogMsg("Database.Sim", DEBUG) << "Created request " << r.rid;

        sdb.requestCache = make_pair(rid, &r);

        SimAppDatabase::totalRequests++;
        SimAppDatabase::totalRequestsMemory +=
                sizeof(SimAppDatabase::Request) + r.tasks.size() * sizeof(SimAppDatabase::RemoteTask *);

        msg.setRequestId(rid);
        msg.setFirstTask(1);
        msg.setLastTask(r.tasks.size());
        msg.setMinRequirements(instance.req);
    }
}


int64_t TaskBagAppDatabase::getInstanceId(int64_t rid) {
    int64_t appId = SimAppDatabase::getAppId(rid);
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    return sdb.instanceCache.first == appId || sdb.instances.count(appId) ? appId : -1;
}


bool TaskBagAppDatabase::startSearch(int64_t rid, Time timeout) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    if (sdb.findRequest(rid)) {
        SimAppDatabase::Request * request = sdb.requestCache.second;
        request->rtime = request->stime = Time::getCurrentTime();
        LogMsg("Database.Sim", DEBUG) << "Submitting request " << rid;
        for (vector<SimAppDatabase::RemoteTask *>::iterator t = request->tasks.begin(); t != request->tasks.end(); ++t)
            if (*t != NULL)
                (*t)->state = SimAppDatabase::RemoteTask::SEARCHING;
        return true;
    }
    return false;
}


unsigned int TaskBagAppDatabase::cancelSearch(int64_t rid) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    unsigned int readyTasks = 0;
    if (sdb.findRequest(rid)) {
        SimAppDatabase::Request * request = sdb.requestCache.second;
        for (vector<SimAppDatabase::RemoteTask *>::iterator t = request->tasks.begin(); t != request->tasks.end(); ++t)
            if (*t != NULL && (*t)->state == SimAppDatabase::RemoteTask::SEARCHING) {
                // If any task was still in searching state, search stops here
                request->stime = Time::getCurrentTime();
                (*t)->state = SimAppDatabase::RemoteTask::READY;
                ++readyTasks;
                *t = NULL;
                --request->remainingTasks;
            }
    }
    LogMsg("Database.Sim", DEBUG) << "Canceled " << readyTasks << " tasks from request " << rid;
    return readyTasks;
}


unsigned int TaskBagAppDatabase::acceptedTasks(const CommAddress & src, int64_t rid, unsigned int firstRtid, unsigned int lastRtid) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    unsigned int accepted = 0;
    if (sdb.findRequest(rid)) {
        SimAppDatabase::Request * request = sdb.requestCache.second;
        LogMsg("Database.Sim", DEBUG) << src << " accepts " << (lastRtid - firstRtid + 1) << " tasks from request " << rid;
        // Check all tasks are in this request
        for (unsigned int t = firstRtid - 1; t < lastRtid; ++t) {
            if (t >= request->tasks.size() || request->tasks[t] == NULL) {
                LogMsg("Database.Sim", INFO) << "No task " << t << " in request with id " << rid;
            } else if (request->tasks[t]->state != SimAppDatabase::RemoteTask::SEARCHING) {
                LogMsg("Database.Sim", INFO) << "Task " << t << " in request " << rid << " not in searching state.";
            } else {
                ++accepted;
                ++request->acceptedTasks;
                request->tasks[t]->state = SimAppDatabase::RemoteTask::EXECUTING;
                request->tasks[t]->host = src;
            }
        }
        if (accepted) {
            // Update last search time
            request->stime = Time::getCurrentTime();
            LogMsg("Database.Sim", DEBUG) << "Update search time to " << (request->stime - request->rtime).seconds() << " seconds";
            request->numNodes++;
        }
    }
    return accepted;
}


bool TaskBagAppDatabase::taskInRequest(unsigned int tid, int64_t rid) {
    tid--;
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    LogMsg("Database.Sim", DEBUG) << "Checking if task " << tid << " is in request " << rid;
    if (sdb.findRequest(rid))
        return sdb.requestCache.second->tasks.size() > tid && sdb.requestCache.second->tasks[tid] != NULL;
    return false;
}


bool TaskBagAppDatabase::finishedTask(const CommAddress & src, int64_t rid, unsigned int rtid) {
    rtid--;
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    if (sdb.findRequest(rid)) {
        SimAppDatabase::Request * request = sdb.requestCache.second;
        LogMsg("Database.Sim", DEBUG) << "Finished task " << rtid << " from request " << rid;
        if (request->tasks.size() <= rtid) return false;
        else if (request->tasks[rtid] == NULL) {
            LogMsg("Database.Sim", WARN) << "Task " << rtid << " of request " << rid << " already finished";
            return false;
        } else {
            request->tasks[rtid]->state = SimAppDatabase::RemoteTask::FINISHED;
            request->tasks[rtid] = NULL;
            --request->remainingTasks;
            return true;
        }
    }
    return false;
}


bool TaskBagAppDatabase::abortedTask(const CommAddress & src, int64_t rid, unsigned int rtid) {
    rtid--;
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    if (sdb.findRequest(rid)) {
        SimAppDatabase::Request * request = sdb.requestCache.second;
        LogMsg("Database.Sim", DEBUG) << src << " aborts task " << rtid << " from request " << rid;
        if (request->tasks.size() <= rtid || request->tasks[rtid] == NULL) {
            return false;
        }
        request->tasks[rtid]->state = SimAppDatabase::RemoteTask::READY;
        request->tasks[rtid] = NULL;
        return true;
    }
    return false;
}


void TaskBagAppDatabase::deadNode(const CommAddress & fail) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    // Make a list of all tasks that where executing in that node
    LogMsg("Database.Sim", DEBUG) << "Node " << fail << " fails, looking for its tasks:";
    for (map<int64_t, SimAppDatabase::AppInstance>::iterator instance = sdb.instances.begin(); instance != sdb.instances.end(); ++instance)
        for (list<SimAppDatabase::Request>::iterator request = instance->second.requests.begin();
                request != instance->second.requests.end(); ++request)
            for (vector<SimAppDatabase::RemoteTask *>::iterator t = request->tasks.begin(); t != request->tasks.end(); ++t) {
                if (*t != NULL && (*t)->state == SimAppDatabase::RemoteTask::EXECUTING && (*t)->host == fail) {
                    // Change their status to READY
                    (*t)->state = SimAppDatabase::RemoteTask::READY;
                    // Take it out of its request
                    *t = NULL;
                }
            }
}


unsigned long int TaskBagAppDatabase::getNumFinished(int64_t appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    unsigned long int result = 0;
    if (sdb.findAppInstance(appId))
        for (vector<SimAppDatabase::RemoteTask>::iterator t = sdb.instanceCache.second->tasks.begin(); t != sdb.instanceCache.second->tasks.end(); t++)
            if (t->state == SimAppDatabase::RemoteTask::FINISHED) result++;
    return result;
}


unsigned long int TaskBagAppDatabase::getNumReady(int64_t appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    unsigned long int result = 0;
    if (sdb.findAppInstance(appId))
        for (vector<SimAppDatabase::RemoteTask>::iterator t = sdb.instanceCache.second->tasks.begin(); t != sdb.instanceCache.second->tasks.end(); t++)
            if (t->state == SimAppDatabase::RemoteTask::READY) result++;
    return result;
}


unsigned long int TaskBagAppDatabase::getNumExecuting(int64_t appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    unsigned long int result = 0;
    if (sdb.findAppInstance(appId))
        for (vector<SimAppDatabase::RemoteTask>::iterator t = sdb.instanceCache.second->tasks.begin(); t != sdb.instanceCache.second->tasks.end(); t++)
            if (t->state == SimAppDatabase::RemoteTask::EXECUTING) result++;
    return result;
}


unsigned long int TaskBagAppDatabase::getNumInProcess(int64_t appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    unsigned long int result = 0;
    if (sdb.findAppInstance(appId))
        for (vector<SimAppDatabase::RemoteTask>::iterator t = sdb.instanceCache.second->tasks.begin(); t != sdb.instanceCache.second->tasks.end(); t++)
            if (t->state == SimAppDatabase::RemoteTask::EXECUTING || t->state == SimAppDatabase::RemoteTask::SEARCHING) result++;
    return result;
}


bool TaskBagAppDatabase::isFinished(int64_t appId) {
    return false;
}

Time TaskBagAppDatabase::getReleaseTime(int64_t appId) {
    SimAppDatabase & sdb = SimAppDatabase::getCurrentDatabase();
    return sdb.findAppInstance(appId) ? sdb.instanceCache.second->ctime : Time();
}
