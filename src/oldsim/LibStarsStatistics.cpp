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

#include "ConfigurationManager.hpp"
#include "LibStarsStatistics.hpp"
#include "Simulator.hpp"
#include "StarsNode.hpp"
#include "Scheduler.hpp"
#include "FSPDispatcher.hpp"


LibStarsStatistics::LibStarsStatistics() : existingTasks(0), runningTasks(0), partialFinishedTasks(0), totalFinishedTasks(0),
        partialComputation(0), totalComputation(0), unfinishedApps(0), totalApps(0) {}


void LibStarsStatistics::openStatsFiles(const boost::filesystem::path & statDir) {
    // Queue statistics
    queueos.open(statDir / boost::filesystem::path("queue_length.stat"));
    queueos << "# Time, max, comment" << std::setprecision(3) << std::fixed << std::endl;

    // Throughput statistics
    throughputos.open(statDir / fs::path("throughput.stat"));
    throughputos << "# Time, tasks finished per second, total tasks finished, computation per second, total computation" << std::endl;
    throughputos << "0,0,0,0,0" << std::endl;

    // App statistics
    appos.open(statDir / fs::path("apps.stat"));
    appos << "# App. ID, src node, num tasks, task size, task mem, task disk, release date, deadline, num finished, JTT, sequential time at src, slowness" << std::endl;
    reqos.open(statDir / fs::path("requests.stat"));
    reqos << "# Req. ID, App. ID, num tasks, num nodes, num accepted, release date, search time" << std::endl;

    lastTSample = Simulator::getInstance().getCurrentTime();
}


// Queue statistics
void Scheduler::addedTasksEvent(const TaskBagMsg & msg, unsigned int numAccepted) {
    Simulator::getInstance().getStarsStatistics().addedTasksEvent(msg, numAccepted);
}


void Scheduler::startedTaskEvent(const Task & t) {
    Simulator::getInstance().getStarsStatistics().taskStarted();
}


void Scheduler::finishedTaskEvent(const Task & t, int oldState, int newState) {
    Simulator::getInstance().getStarsStatistics().taskFinished(t, oldState, newState);
}


void LibStarsStatistics::addedTasksEvent(const TaskBagMsg & msg, unsigned int numAccepted) {
    Simulator & sim = Simulator::getInstance();
    existingTasks += numAccepted;
    CommAddress a = sim.getCurrentNode().getLocalAddress();
    Time now = sim.getCurrentTime(), queueEnd = now;
    auto & tasks = sim.getCurrentNode().getSch().getTasks();
    for (auto it = tasks.begin(); it != tasks.end(); it++) {
        queueEnd += (*it)->getEstimatedDuration();
    }
    // Record maximum queue length
    if (maxQueue < queueEnd) {
        queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds()
                << ",queue length updated" << std::endl;
        maxQueue = queueEnd;
        queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds()
                << ',' << numAccepted << " new tasks accepted at " << a
                << " for request " << msg.getRequestId() << std::endl;
    }
}


void LibStarsStatistics::finishQueueLengthStatistics() {
    Time now = Simulator::getInstance().getCurrentTime();
    queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << ",end" << std::endl;
}


void LibStarsStatistics::finishThroughputStatistics() {
    Time now = Simulator::getInstance().getCurrentTime();
    double elapsed = (now - lastTSample).seconds();
    throughputos << std::setprecision(3) << std::fixed << (now.getRawDate() / 1000000.0) << ','
            << (partialFinishedTasks / elapsed) << ','
            << totalFinishedTasks << ','
            << (partialComputation / elapsed) << ','
            << totalComputation
            << std::endl;
}


void LibStarsStatistics::taskStarted() {
    ++runningTasks;
}


void LibStarsStatistics::taskFinished(const Task & t, int oldState, int newState) {
    --existingTasks;
    if (oldState == Task::Running)
        --runningTasks;
    // Record throughput
    if (newState == Task::Finished) {
        Time now = Time::getCurrentTime();
        partialFinishedTasks++;
        partialComputation += t.getDescription().getLength();
        totalFinishedTasks++;
        totalComputation += t.getDescription().getLength();
        double elapsed = (now - lastTSample).seconds();
        if (elapsed >= delayTSample) {
            throughputos << std::setprecision(3) << std::fixed << (now.getRawDate() / 1000000.0) << ','
                    << (partialFinishedTasks / elapsed) << ','
                    << totalFinishedTasks << ','
                    << (partialComputation / elapsed) << ','
                    << totalComputation
                    << std::endl;
            partialFinishedTasks = 0;
            partialComputation = 0;
            lastTSample = now;
        }
    }
}


void LibStarsStatistics::saveCPUStatistics() {
    Simulator & sim = Simulator::getInstance();
    fs::ofstream os(sim.getResultDir() / fs::path("cpu.stat"));
    os << std::setprecision(6) << std::fixed;

    uint16_t port = ConfigurationManager::getInstance().getPort();
    os << "# Node, tasks exec'd, max queue, max paused, power, mem, disk" << std::endl;
    for (unsigned long int addr = 0; addr < sim.getNumNodes(); ++addr) {
        StarsNode & node = sim.getNode(addr);
        unsigned int executedTasks = node.getSch().getExecutedTasks();
        os << CommAddress(addr, port) << ',' << executedTasks << ','
                << node.getSch().getMaxQueueLength() << ','
                << node.getSch().getMaxPausedTasks() << ','
                << node.getAveragePower() << ',' << node.getAvailableMemory() << ',' << node.getAvailableDisk() << std::endl;
    }
}


void SubmissionNode::finishedApp(int64_t appId) {
    Simulator & sim = Simulator::getInstance();
    sim.getStarsStatistics().finishedApp(sim.getCurrentNode(), appId, sim.getCurrentTime(), 0);
    sim.getSimulationCase()->finishedApp(appId);
}


void LibStarsStatistics::finishedApp(StarsNode & node, int64_t appId, Time end, int finishedTasks) {
    SimAppDatabase & sdb = node.getDatabase();

    totalApps++;

    // Show app instance information
    const SimAppDatabase::AppInstance * app = sdb.getAppInstance(appId);
    if (app == NULL) {
        Logger::msg("Sim.stat", WARN, "Application ", appId, " at node ", node.getLocalAddress(), " does not exist");
        return;
    }
    double jtt = (end - app->ctime).seconds();
    double sequential = (double)app->req.getAppLength() / node.getAveragePower();
    double slowness = jtt / app->req.getLength();
    for (std::vector<SimAppDatabase::RemoteTask>::const_iterator it = app->tasks.begin(); it != app->tasks.end(); it++)
        if (it->state == Task::Finished) finishedTasks++;
    if (finishedTasks == 0)
        unfinishedApps++;
    appos << appId << ',' << node.getLocalAddress() << ','
        << app->req.getNumTasks() << ',' << app->req.getLength() << ','
        << app->req.getMaxMemory() << ',' << app->req.getMaxDisk() << ','
        << std::setprecision(3) << std::fixed << (app->ctime.getRawDate() / 1000000.0) << ','
        << (app->req.getDeadline().getRawDate() / 1000000.0) << ','
        << finishedTasks << ',' << jtt << ',' << sequential << ','
        << std::setprecision(8) << std::fixed << slowness << std::endl;

    // Show requests information
    const std::list<SimAppDatabase::Request> requests = app->requests;
    for (std::list<SimAppDatabase::Request>::const_iterator it = requests.begin(); it != requests.end(); ++it) {
        double search = (it->stime - it->rtime).seconds();
        reqos << it->rid << ',' << appId << ',' << node.getLocalAddress() << ','
            << it->tasks.size() << ',' << it->countNodes() << ',' << it->acceptedTasks << ','
            << std::setprecision(3) << std::fixed << (it->rtime.getRawDate() / 1000000.0) << ','
            << std::setprecision(8) << std::fixed << search << std::endl;
    }
}


struct UnfinishedApp {
    uint32_t node;
    int64_t appid;
    Time end;
    unsigned int finishedTasks;
    UnfinishedApp(uint32_t n, int64_t a, Time t, unsigned int f) : node(n), appid(a), end(t), finishedTasks(f) {}
    bool operator<(const UnfinishedApp & r) const { return end < r.end; }
};


std::vector<std::map<int64_t, std::pair<Time, int> > > LibStarsStatistics::getUnfinishedTasksPerNode() {
    // Calculate expected end time and remaining tasks of each application in course
    Simulator & sim = Simulator::getInstance();
    Time now = sim.getCurrentTime();
    std::vector<std::map<int64_t, std::pair<Time, int> > > unfinishedAppsPerNode(sim.getNumNodes());
    for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
        // Get the task queue
        std::list<std::shared_ptr<Task> >& tasks = sim.getNode(n).getSch().getTasks();
        Time end = now;
        for (auto t = tasks.begin(); t != tasks.end(); ++t) {
            // Get its app, add a finished task and check its finish time
            end += (*t)->getEstimatedDuration();
            uint32_t origin = (*t)->getOwner().getIPNum();
            int64_t appId = SimAppDatabase::getAppId((*t)->getClientRequestId());
            auto& unfinishedTasks = unfinishedAppsPerNode[origin][appId];
            if (unfinishedTasks.first < end) {
                unfinishedTasks.first = end;
            }
            ++unfinishedTasks.second;
        }
    }
    return unfinishedAppsPerNode;
}


void LibStarsStatistics::finishAppStatistics() {
    Simulator & sim = Simulator::getInstance();
    auto unfinishedAppsPerNode = getUnfinishedTasksPerNode();
    for (unsigned int n = 0; n < sim.getNumNodes(); ++n) {
        for (auto & j : unfinishedAppsPerNode[n]) {
            finishedApp(sim.getNode(n), j.first, j.second.first, j.second.second);
        }
    }
    appos << "# " << totalApps << " jobs finished at simulation end of which " << unfinishedApps << " (" << std::setprecision(2) << std::fixed
        << ((unfinishedApps * 100.0) / totalApps) << "%) didn't get any task finished." << std::endl;
}
