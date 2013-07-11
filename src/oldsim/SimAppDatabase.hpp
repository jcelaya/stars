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

#ifndef SIMAPPDATABASE_H_
#define SIMAPPDATABASE_H_

#include <map>
#include <list>
#include <string>
#include <vector>
#include "TaskBagAppDatabase.hpp"
#include "CommAddress.hpp"

class SimAppDatabase {
public:
    struct RemoteTask {
        enum {
            READY,
            SEARCHING,
            EXECUTING,
            FINISHED
        } state;
        CommAddress host;
        RemoteTask() : state(READY) {}
    };

    struct Request {
        int64_t rid;
        Time rtime, stime;
        size_t acceptedTasks, remainingTasks;
        std::vector<RemoteTask *> tasks;
//        friend std::ostream & operator<<(std::ostream& os, const Request & r) {
//            return os << "(A=" << r.appId << ", r=" << r.rtime << ", s=" << r.stime
//                    << ", n=" << r.numNodes << ", a=" << r.acceptedTasks << ", t=" << r.tasks.size() << ")";
//        }
        size_t countNodes() const {
            std::list<CommAddress> hosts;
            for (auto & task : tasks) {
                if (task)
                    hosts.push_back(task->host);
            }
            hosts.sort();
            hosts.unique();
            return hosts.size();
        }
    };

    struct AppInstance {
        TaskDescription req;
        Time ctime;
        std::vector<RemoteTask> tasks;
        std::list<Request> requests;
    };

    SimAppDatabase() : lastInstance(0) {
        instanceCache.first = requestCache.first = -1;
    }

    void setNextApp(const TaskDescription & req) { nextApp = req; }
    const TaskDescription & getNextApp() const { return nextApp; }
    void appInstanceFinished(int64_t appId);
    const AppInstance * getAppInstance(int64_t appId) { return findAppInstance(appId) ? instanceCache.second : NULL; }
    void updateDeadline(int64_t appId, Time nd) { instances.find(appId)->second.req.setDeadline(nd); }

    static SimAppDatabase & getCurrentDatabase();
    static void reset() {
        totalInstances = totalInstancesMemory = totalRequests = totalRequestsMemory = 0;
    }
    static int64_t getAppId(int64_t rid) { return rid >> bitsPerRequest; }

    static unsigned long int getTotalInstances() { return totalInstances; }
    static unsigned long int getTotalInstancesMem() { return totalInstancesMemory; }
    static unsigned long int getTotalRequests() { return totalRequests; }
    static unsigned long int getTotalRequestsMem() { return totalRequestsMemory; }

private:
    friend class TaskBagAppDatabase;
    static const int bitsPerRequest = 32;
    static unsigned long int totalInstances, totalInstancesMemory;
    static unsigned long int totalRequests, totalRequestsMemory;

    TaskDescription nextApp;
    std::map<int64_t, AppInstance> instances;
    int64_t lastInstance;
    std::pair<int64_t, AppInstance *> instanceCache;
    std::pair<int64_t, Request *> requestCache;

    bool findAppInstance(int64_t appId);
    bool findRequest(int64_t rid);
};

#endif /* SIMAPPDATABASE_H_ */
