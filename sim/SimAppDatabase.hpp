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
    struct AppInstance {
        struct Task {
            enum {
                READY,
                SEARCHING,
                EXECUTING,
                FINISHED
            } state;
            Time atime, ftime;
            CommAddress host;
            Task() : state(READY) {}
        };

        TaskDescription req;
        Time ctime, rtime;
        std::vector<Task> tasks;
    };

    struct Request {
        long int appId;
        Time rtime, stime;
        long int numNodes, acceptedTasks;
        std::vector<AppInstance::Task *> tasks;
        friend std::ostream & operator<<(std::ostream& os, const Request & r) {
            return os << "(A=" << r.appId << ", r=" << r.rtime << ", s=" << r.stime
                   << ", n=" << r.numNodes << ", a=" << r.acceptedTasks << ", t=" << r.tasks.size() << ")";
        }
    };

private:
    friend class TaskBagAppDatabase;

    std::pair<std::string, TaskDescription> lastApp;
    std::map<std::string, TaskDescription> apps;
    std::map<long int, AppInstance> instances;
    std::map<long int, Request> requests;

    static unsigned long int totalApps, totalAppsMemory;
    static unsigned long int totalInstances, totalInstancesMemory;
    static unsigned long int totalRequests, totalRequestsMemory;

    static long int lastInstance, lastRequest;

    friend std::ostream & operator<<(std::ostream& os, const SimAppDatabase & s);

public:

    void createAppDescription(const std::string & name, const TaskDescription & req);
    void dropAppDescription(const std::string & name);
    const std::pair<std::string, TaskDescription> & getLastApp() const {
        return lastApp;
    }
    void appInstanceFinished(long int appId);
    bool appInstanceExists(long int appId) {
        return instances.count(appId) > 0;
    }
    const AppInstance & getAppInstance(long int appId) const {
        return instances.find(appId)->second;
    }
    long int getAppId(long int rid) const {
        std::map<long int, Request>::const_iterator it = requests.find(rid);
        return it != requests.end() ? it->second.appId : -1;
    }
    bool getNextAppRequest(long int appId, std::map<long int, Request>::const_iterator & it, bool valid);
    void updateDeadline(long int appId, Time nd) {
        instances.find(appId)->second.req.setDeadline(nd);
    }

    static SimAppDatabase & getCurrentDatabase();
    static void reset() {
        totalApps = totalAppsMemory = totalInstances = totalInstancesMemory = totalRequests = totalRequestsMemory = lastInstance = lastRequest = 0;
    }
    static long int getLastInstance() {
        return lastInstance;
    }
    static unsigned long int getTotalApps() {
        return totalApps;
    }
    static unsigned long int getTotalAppsMem() {
        return totalAppsMemory;
    }
    static unsigned long int getTotalInstances() {
        return totalInstances;
    }
    static unsigned long int getTotalInstancesMem() {
        return totalInstancesMemory;
    }
    static unsigned long int getTotalRequests() {
        return totalRequests;
    }
    static unsigned long int getTotalRequestsMem() {
        return totalRequestsMemory;
    }
};

#endif /* SIMAPPDATABASE_H_ */