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
#include "CentralizedScheduler.hpp"


LibStarsStatistics::LibStarsStatistics() : existingTasks(0), partialFinishedTasks(0), totalFinishedTasks(0),
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
    slowos.open(statDir / fs::path("slowness.stat"));
    slowos << "# Time, maximum slowness" << std::endl;
    lastTSample = Simulator::getInstance().getCurrentTime();
}


// Queue statistics
void Scheduler::queueChangedStatistics(int64_t rid, unsigned int numAccepted, Time queueEnd) {
    Simulator::getInstance().getStarsStatistics().queueChangedStatistics(rid, numAccepted, queueEnd);
}


void LibStarsStatistics::queueChangedStatistics(int64_t rid, unsigned int numAccepted, Time queueEnd) {
    Time now = Simulator::getInstance().getCurrentTime();
    if (maxQueue < queueEnd) {
        queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds()
                << ",queue length updated" << std::endl;
        maxQueue = queueEnd;
        queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds()
                << ',' << numAccepted << " new tasks accepted at " << Simulator::getInstance().getCurrentNode().getLocalAddress()
                << " for request " << rid << std::endl;
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
    ++existingTasks;
}


void LibStarsStatistics::taskFinished(const Task & t, bool successful) {
    --existingTasks;
    if (successful) {
        partialFinishedTasks++;
        partialComputation += t.getDescription().getLength();
        totalFinishedTasks++;
        totalComputation += t.getDescription().getLength();
        Time now = Simulator::getInstance().getCurrentTime();
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

    unsigned int maxTasks = 0;
    uint16_t port = ConfigurationManager::getInstance().getPort();
    os << "# Node, tasks exec'd" << std::endl;
    for (unsigned long int addr = 0; addr < sim.getNumNodes(); ++addr) {
        StarsNode & node = sim.getNode(addr);
        unsigned int executedTasks = node.getScheduler().getExecutedTasks();
        os << CommAddress(addr, port) << ',' << executedTasks << ','
                << node.getAveragePower() << ',' << node.getAvailableMemory() << ',' << node.getAvailableDisk() << std::endl;
        if (executedTasks > maxTasks) maxTasks = executedTasks;
    }
}


void LibStarsStatistics::finishedApp(StarsNode & node, int64_t appId, Time end, int finishedTasks) {
    SimAppDatabase & sdb = node.getDatabase();

    totalApps++;

    // Show app instance information
    const SimAppDatabase::AppInstance * app = sdb.getAppInstance(appId);
    if (app == NULL) {
        LogMsg("Sim.stat", WARN) << "Application " << appId << " at node " << node.getLocalAddress() << " does not exist";
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
            << it->tasks.size() << ',' << it->numNodes << ',' << it->acceptedTasks << ','
            << std::setprecision(3) << std::fixed << (it->rtime.getRawDate() / 1000000.0) << ','
            << std::setprecision(8) << std::fixed << search << std::endl;
    }

    // Save maximum slowness among concurrently running applications
    Time start = app->ctime;
    // Record the maximum of all the apps that ended before "start"
    while (!lastSlowness.empty() && lastSlowness.front().first < start) {
        double maxSlowness = 0.0;
        for (std::list<std::pair<Time, double> >::iterator it = lastSlowness.begin(); it != lastSlowness.end(); ++it)
            if (maxSlowness < it->second) maxSlowness = it->second;
        slowos << std::setprecision(3) << std::fixed << (lastSlowness.front().first.getRawDate() / 1000000.0) << ','
            << std::setprecision(8) << std::fixed << maxSlowness << std::endl;
        lastSlowness.pop_front();
    }
    // Now record the maximum slowness among this app and the apps that ended after "start"
    double maxSlowness = slowness;
    for (std::list<std::pair<Time, double> >::iterator it = lastSlowness.begin(); it != lastSlowness.end(); ++it)
        if (maxSlowness < it->second) maxSlowness = it->second;
    lastSlowness.push_back(std::make_pair(end, maxSlowness));
}


struct UnfinishedApp {
    uint32_t node;
    int64_t appid;
    Time end;
    unsigned int finishedTasks;
    UnfinishedApp(uint32_t n, int64_t a, Time t, unsigned int f) : node(n), appid(a), end(t), finishedTasks(f) {}
    bool operator<(const UnfinishedApp & r) const { return end < r.end; }
};


void LibStarsStatistics::finishAppStatistics() {
    Simulator & sim = Simulator::getInstance();
    Time now = sim.getCurrentTime();

    // Unfinished jobs
    // Calculate expected end time and remaining tasks of each application in course
    std::vector<std::map<int64_t, std::pair<Time, int> > > unfinishedAppsPerNode(sim.getNumNodes());
    boost::shared_ptr<CentralizedScheduler> ps = sim.getCentralizedScheduler();
    if (ps.get()) {
        for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
            // Get the task queue
            const std::list<CentralizedScheduler::TaskDesc> & tasks = ps->getQueue(n);
            Time end = now;
            // For each task...
            for (std::list<CentralizedScheduler::TaskDesc>::const_iterator t = tasks.begin(); t != tasks.end(); t++) {
                // Get its app, add a finished task and check its finish time
                end += t->a;
                uint32_t origin = t->msg->getRequester().getIPNum();
                int64_t appId = SimAppDatabase::getAppId(t->msg->getRequestId());
                std::map<int64_t, std::pair<Time, int> >::iterator j = unfinishedAppsPerNode[origin].find(appId);
                if (j == unfinishedAppsPerNode[origin].end())
                    unfinishedAppsPerNode[origin][appId] = std::make_pair(end, 1);
                else {
                    if (j->second.first < end) j->second.first = end;
                    ++(j->second.second);
                }
            }
        }
    } else {
        for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
            // Get the task queue
            std::list<boost::shared_ptr<Task> > & tasks = sim.getNode(n).getScheduler().getTasks();
            Time end = now;
            // For each task...
            for (std::list<boost::shared_ptr<Task> >::iterator t = tasks.begin(); t != tasks.end(); t++) {
                // Get its app, add a finished task and check its finish time
                end += (*t)->getEstimatedDuration();
                uint32_t origin = (*t)->getOwner().getIPNum();
                int64_t appId = SimAppDatabase::getAppId((*t)->getClientRequestId());
                std::map<int64_t, std::pair<Time, int> >::iterator j = unfinishedAppsPerNode[origin].find(appId);
                if (j == unfinishedAppsPerNode[origin].end())
                    unfinishedAppsPerNode[origin][appId] = std::make_pair(end, 1);
                else {
                    if (j->second.first < end) j->second.first = end;
                    ++(j->second.second);
                }
            }
        }
     }
    // Take into account centralized scheduler queues

    // Reorder unfinished apps
    std::list<UnfinishedApp> sortedApps;
    for (unsigned int n = 0; n < sim.getNumNodes(); ++n) {
        for (std::map<int64_t, std::pair<Time, int> >::iterator j = unfinishedAppsPerNode[n].begin(); j != unfinishedAppsPerNode[n].end(); j++) {
            sortedApps.push_back(UnfinishedApp(n, j->first, j->second.first, j->second.second));
        }
    }
    sortedApps.sort();

    // Write its data and requests
    for (std::list<UnfinishedApp>::iterator it = sortedApps.begin(); it != sortedApps.end(); ++it) {
        finishedApp(sim.getNode(it->node), it->appid, it->end, it->finishedTasks);
    }

    // Finished percentages
    appos << "# " << totalApps << " jobs finished at simulation end of which " << unfinishedApps << " (" << std::setprecision(2) << std::fixed
        << ((unfinishedApps * 100.0) / totalApps) << "%) didn't get any task finished." << std::endl;

    // Write the slowness data of the last apps
    while (!lastSlowness.empty()) {
        double maxSlowness = 0.0;
        for (std::list<std::pair<Time, double> >::iterator it = lastSlowness.begin(); it != lastSlowness.end(); ++it)
            if (maxSlowness < it->second) maxSlowness = it->second;
        slowos << std::setprecision(3) << std::fixed << (lastSlowness.front().first.getRawDate() / 1000000.0) << ','
            << std::setprecision(8) << std::fixed << maxSlowness << std::endl;
        lastSlowness.pop_front();
    }
}
