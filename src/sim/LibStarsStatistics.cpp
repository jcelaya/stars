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

#include <vector>
#include "ConfigurationManager.hpp"
#include "LibStarsStatistics.hpp"
#include "Simulator.hpp"
#include "Scheduler.hpp"
#include "StarsNode.hpp"


LibStarsStatistics::LibStarsStatistics() :
    existingTasks(0), partialFinishedTasks(0), totalFinishedTasks(0),
    numNodesHist(1.0), finishedHist(0.1), searchHist(100U),
    jttHist(100U), seqHist(100U), spupHist(0.1), slownessHist(100U), unfinishedApps(0), totalApps(0) {
}


void LibStarsStatistics::openStatsFiles() {
    const boost::filesystem::path & statDir = Simulator::getInstance().getResultDir();
    // Queue statistics
    queueos.open(statDir / boost::filesystem::path("queue_length.stat"));
    queueos << "# Time, max, comment" << std::setprecision(3) << std::fixed << std::endl;

    // Throughput statistics
    throughputos.open(statDir / fs::path("throughput.stat"));
    throughputos << "# Time, tasks finished per second, total tasks finished" << std::endl;
    throughputos << "0,0,0" << std::endl;

    // App statistics
    appos.open(statDir / fs::path("apps.stat"));
    appos << "# App. ID, src node, num tasks, task size, task mem, task disk, release date, deadline, num finished, JTT, sequential time at src, slowness" << std::endl;
    reqos.open(statDir / fs::path("requests.stat"));
    reqos << "# Req. ID, App. ID, num tasks, num nodes, num accepted, release date, search time" << std::endl;
    slowos.open(statDir / fs::path("slowness.stat"));
    slowos << "# Time, maximum slowness" << std::endl;
}


// Queue statistics
void Scheduler::queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd) {
    Simulator::getInstance().getStarsStatistics().queueChangedStatistics(rid, numAccepted, queueEnd);
}


void LibStarsStatistics::queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd) {
    boost::mutex::scoped_lock lock(m);
    Time now = Time::getCurrentTime();
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
    Time now = Time::getCurrentTime();
    queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << ",end" << std::endl;
}


void LibStarsStatistics::finishThroughputStatistics() {
    Time now = Time::getCurrentTime();
    double elapsed = (now - lastTSample).seconds();
    throughputos << std::setprecision(3) << std::fixed << (now.getRawDate() / 1000000.0) << ',' << (partialFinishedTasks / elapsed) << ',' << totalFinishedTasks << std::endl;
}


void LibStarsStatistics::taskStarted() {
    boost::mutex::scoped_lock lock(m);
    ++existingTasks;
}


void LibStarsStatistics::taskFinished(bool successful) {
    boost::mutex::scoped_lock lock(m);
    --existingTasks;
    if (successful) {
        partialFinishedTasks++;
        totalFinishedTasks++;
        Time now = Time::getCurrentTime();
        double elapsed = (now - lastTSample).seconds();
        if (elapsed >= delayTSample) {
            throughputos << std::setprecision(3) << std::fixed << (now.getRawDate() / 1000000.0) << ',' << (partialFinishedTasks / elapsed) << ',' << totalFinishedTasks << std::endl;
            partialFinishedTasks = 0;
            lastTSample = now;
        }
    }
}


void LibStarsStatistics::saveCPUStatistics() {
    Simulator & sim = Simulator::getInstance();
    boost::filesystem::ofstream os(sim.getResultDir() / boost::filesystem::path("cpu.stat"));
    os << std::setprecision(6) << std::fixed;

    unsigned int maxTasks = 0;
    uint16_t port = ConfigurationManager::getInstance().getPort();
    os << "# Node, tasks exec'd" << std::endl;
    for (unsigned long int addr = 0; addr < sim.getNumNodes(); ++addr) {
        unsigned int executedTasks = sim.getNode(addr).getScheduler().getExecutedTasks();
        os << CommAddress(addr, port) << ',' << executedTasks << std::endl;
        if (executedTasks > maxTasks) maxTasks = executedTasks;
    }
    // End this index
    os << std::endl << std::endl;

    // Next blocks are CDFs
    Histogram executedTasks(maxTasks);
    for (unsigned long int addr = 0; addr < sim.getNumNodes(); ++addr) {
        executedTasks.addValue(sim.getNode(addr).getScheduler().getExecutedTasks());
    }
    os << "# CDF of num of executed tasks" << std::endl << CDF(executedTasks) << std::endl << std::endl;
}


void SubmissionNode::finishedApp(long int appId) {
    Simulator & sim = Simulator::getInstance();
    sim.getStarsStatistics().finishedApp(sim.getCurrentNode(), appId, Time::getCurrentTime(), 0);
    sim.getCase().finishedApp(appId);
}


void LibStarsStatistics::finishedApp(StarsNode & node, long int appId, Time end, int finishedTasks) {
    boost::mutex::scoped_lock lock(m);
    SimAppDatabase & sdb = node.getDatabase();
    if (!sdb.appInstanceExists(appId))
        return; // The application was never sent?

    totalApps++;

    // Show app instance information
    const SimAppDatabase::AppInstance & app = sdb.getAppInstance(appId);
    double jtt = (end - app.ctime).seconds();
    double sequential = (double)app.req.getAppLength() / node.getAveragePower();
    double slowness = 0.0;
    for (std::vector<SimAppDatabase::AppInstance::Task>::const_iterator it = app.tasks.begin(); it != app.tasks.end(); it++)
        if (it->state == Task::Finished) finishedTasks++;
    finishedHist.addValue((finishedTasks * 100.0) / app.req.getNumTasks());
    if (finishedTasks > 0) {
        // Only count jobs with at least one finished task
        jttHist.addValue(jtt);
        seqHist.addValue(sequential);
        double speedup = sequential * finishedTasks / app.req.getNumTasks() / jtt;
        spupHist.addValue(speedup);
        slowness = jtt / app.req.getLength();
        slownessHist.addValue(slowness);
    } else unfinishedApps++;
    appos << appId << ',' << node.getLocalAddress() << ','
        << app.req.getNumTasks() << ',' << app.req.getLength() << ','
        << app.req.getMaxMemory() << ',' << app.req.getMaxDisk() << ','
        << std::setprecision(3) << std::fixed << (app.ctime.getRawDate() / 1000000.0) << ','
        << (app.req.getDeadline().getRawDate() / 1000000.0) << ','
        << finishedTasks << ',' << jtt << ',' << sequential << ','
        << std::setprecision(8) << std::fixed << slowness << std::endl;

    // Show requests information
    bool valid = false;
    std::map<long int, SimAppDatabase::Request>::const_iterator it;
    while ((valid = sdb.getNextAppRequest(appId, it, valid))) {
        numNodesHist.addValue(it->second.numNodes);
        double search = (it->second.stime - it->second.rtime).seconds();
        searchHist.addValue(search);
        reqos << it->first << ',' << appId << ','
            << it->second.tasks.size() << ',' << it->second.numNodes << ',' << it->second.acceptedTasks << ','
            << std::setprecision(3) << std::fixed << (it->second.rtime.getRawDate() / 1000000.0) << ','
            << std::setprecision(8) << std::fixed << search << std::endl;
    }

    // Save maximum slowness among concurrently running applications
    Time start = app.ctime;
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
    long int appid;
    Time end;
    unsigned int finishedTasks;
    UnfinishedApp(uint32_t n, long int a, Time t, unsigned int f) : node(n), appid(a), end(t), finishedTasks(f) {}
    bool operator<(const UnfinishedApp & r) const { return end < r.end; }
};


void LibStarsStatistics::finishAppStatistics() {
    Simulator & sim = Simulator::getInstance();
    Time now = Time::getCurrentTime();

    // Unfinished jobs
    // Calculate expected end time and remaining tasks of each application in course
    std::vector<std::map<long int, std::pair<Time, int> > > unfinishedAppsPerNode(sim.getNumNodes());
//     boost::shared_ptr<PerfectScheduler> ps = sim.getPerfectScheduler();
//     if (ps.get()) {
//         for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
//             // Get the task queue
//             const list<PerfectScheduler::TaskDesc> & tasks = ps->getQueue(n);
//             Time end = now;
//             // For each task...
//             for (list<PerfectScheduler::TaskDesc>::const_iterator t = tasks.begin(); t != tasks.end(); t++) {
//                 // Get its app, add a finished task and check its finish time
//                 end += t->a;
//                 uint32_t origin = t->msg->getRequester().getIPNum();
//                 SimAppDatabase & sdb = sim.getNode(origin).getDatabase();
//                 long int appId = sdb.getAppId(t->msg->getRequestId());
//                 if (appId >= 0) {
    //                     std::map<long int, std::pair<Time, int> >::iterator j = unfinishedAppsPerNode[origin].find(appId);
    //                     if (j == unfinishedAppsPerNode[origin].end())
    //                         unfinishedAppsPerNode[origin][appId] = make_pair(end, 1);
//                     else {
//                         if (j->second.first < end) j->second.first = end;
//                                 ++(j->second.second);
//                     }
//                 }
//             }
//         }
//     } else {
        for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
            // Get the task queue
            std::list<boost::shared_ptr<Task> > & tasks = sim.getNode(n).getScheduler().getTasks();
            Time end = now;
            // For each task...
            for (std::list<boost::shared_ptr<Task> >::iterator t = tasks.begin(); t != tasks.end(); t++) {
                // Get its app, add a finished task and check its finish time
                end += (*t)->getEstimatedDuration();
                uint32_t origin = (*t)->getOwner().getIPNum();
                SimAppDatabase & sdb = sim.getNode(origin).getDatabase();
                long int appId = sdb.getAppId((*t)->getClientRequestId());
                if (appId >= 0) {
                    std::map<long int, std::pair<Time, int> >::iterator j = unfinishedAppsPerNode[origin].find(appId);
                    if (j == unfinishedAppsPerNode[origin].end())
                        unfinishedAppsPerNode[origin][appId] = std::make_pair(end, 1);
                    else {
                        if (j->second.first < end) j->second.first = end;
                            ++(j->second.second);
                    }
                }
            }
        }
//     }
    // Take into account perfect scheduler queues

    // Reorder unfinished apps
    std::list<UnfinishedApp> sortedApps;
    for (unsigned int n = 0; n < sim.getNumNodes(); ++n) {
        for (std::map<long int, std::pair<Time, int> >::iterator j = unfinishedAppsPerNode[n].begin(); j != unfinishedAppsPerNode[n].end(); j++) {
            sortedApps.push_back(UnfinishedApp(n, j->first, j->second.first, j->second.second));
        }
    }
    sortedApps.sort();

    // Write its data and requests
    for (std::list<UnfinishedApp>::iterator it = sortedApps.begin(); it != sortedApps.end(); ++it) {
        finishedApp(sim.getNode(it->node), it->appid, it->end, it->finishedTasks);
    }
    // Write the slowness data of the last apps
    while (!lastSlowness.empty()) {
        double maxSlowness = 0.0;
        for (std::list<std::pair<Time, double> >::iterator it = lastSlowness.begin(); it != lastSlowness.end(); ++it)
            if (maxSlowness < it->second) maxSlowness = it->second;
        slowos << std::setprecision(3) << std::fixed << (lastSlowness.front().first.getRawDate() / 1000000.0) << ','
            << std::setprecision(8) << std::fixed << maxSlowness << std::endl;
        lastSlowness.pop_front();
    }

    // Finish this index
    appos << std::endl << std::endl;
    // Finished percentages
    appos << totalApps << " jobs finished at simulation end of which " << unfinishedApps << " (" << std::setprecision(2) << std::fixed
        << ((unfinishedApps * 100.0) / totalApps) << "%) didn't get any task finished." << std::endl << std::endl << std::endl;
    appos.precision(8);
    appos << "# Finished % CDF" << std::endl << CDF(finishedHist) << std::endl << std::endl;
    appos << "# JTT CDF" << std::endl << CDF(jttHist) << std::endl << std::endl;
    appos << "# Sequential time in src CDF" << std::endl << CDF(seqHist) << std::endl << std::endl;
    appos << "# Speedup CDF" << std::endl << CDF(spupHist) << std::endl << std::endl;
    appos << "# Slowness CDF" << std::endl << CDF(slownessHist) << std::endl << std::endl;

    // Finish this index
    reqos << std::endl << std::endl;
    reqos.precision(8);
    reqos << "# Number of nodes CDF" << std::endl << CDF(numNodesHist) << std::endl << std::endl;
    reqos << "# Search time CDF" << std::endl << CDF(searchHist) << std::endl << std::endl;
}
