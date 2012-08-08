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

#include <map>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "JobStatistics.hpp"
#include "SimAppDatabase.hpp"
#include "TaskMonitorMsg.hpp"
#include "TaskBagMsg.hpp"
#include "CommAddress.hpp"
#include "Scheduler.hpp"
#include "Logger.hpp"
#include "StarsNode.hpp"
#include "AppFinishedMsg.hpp"
#include "PerfectScheduler.hpp"
using namespace std;
using namespace boost::posix_time;


JobStatistics::JobStatistics() : Simulator::InterEventHandler(), numNodesHist(1.0), finishedHist(0.1), searchHist(100U),
        jttHist(100U), seqHist(100U), spupHist(0.1), slownessHist(100U), unfinishedJobs(0), totalJobs(0) {
    fs::path statDir = sim.getResultDir();

    jos.open(statDir / fs::path("apps.stat"));
    jos << "# App. ID, src node, num tasks, task size, task mem, task disk, release date, deadline, num finished, JTT, sequential time at src, slowness" << endl;
    ros.open(statDir / fs::path("requests.stat"));
    ros << "# Req. ID, App. ID, num tasks, num nodes, num accepted, release date, search time" << endl;
    sos.open(statDir / fs::path("slowness.stat"));
    sos << "# Time, maximum slowness" << endl;
}


void JobStatistics::finishApp(uint32_t node, long int appId, Time end, int finishedTasks) {
    SimAppDatabase & sdb = sim.getNode(node).getDatabase();
    if (!sdb.appInstanceExists(appId))
        return; // The application was never sent?

    totalJobs++;

    // Show app instance information
    const SimAppDatabase::AppInstance & app = sdb.getAppInstance(appId);
    double jtt = (end - app.ctime).seconds();
    double sequential = (double)app.req.getAppLength() / sim.getNode(node).getAveragePower();
    double slowness = 0.0;
    for (vector<SimAppDatabase::AppInstance::Task>::const_iterator it = app.tasks.begin(); it != app.tasks.end(); it++)
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
    } else unfinishedJobs++;
    jos << appId << ',' << CommAddress(node, ConfigurationManager::getInstance().getPort()) << ','
        << app.req.getNumTasks() << ',' << app.req.getLength() << ','
        << app.req.getMaxMemory() << ',' << app.req.getMaxDisk() << ','
        << setprecision(3) << fixed << (app.ctime.getRawDate() / 1000000.0) << ','
        << (app.req.getDeadline().getRawDate() / 1000000.0) << ','
        << finishedTasks << ',' << jtt << ',' << sequential << ','
        << setprecision(8) << fixed << slowness << endl;

    // Show requests information
    bool valid = false;
    std::map<long int, SimAppDatabase::Request>::const_iterator it;
    while ((valid = sdb.getNextAppRequest(appId, it, valid))) {
        numNodesHist.addValue(it->second.numNodes);
        double search = (it->second.stime - it->second.rtime).seconds();
        searchHist.addValue(search);
        ros << it->first << ',' << appId << ','
            << it->second.tasks.size() << ',' << it->second.numNodes << ',' << it->second.acceptedTasks << ','
            << setprecision(3) << fixed << (it->second.rtime.getRawDate() / 1000000.0) << ','
            << setprecision(8) << fixed << search << endl;
    }

    // Save maximum slowness among concurrently running applications
    Time start = app.ctime;
    // Record the maximum of all the apps that ended before "start"
    while (!lastSlowness.empty() && lastSlowness.front().first < start) {
        double maxSlowness = 0.0;
        for (std::list<std::pair<Time, double> >::iterator it = lastSlowness.begin(); it != lastSlowness.end(); ++it)
            if (maxSlowness < it->second) maxSlowness = it->second;
        sos << setprecision(3) << fixed << (lastSlowness.front().first.getRawDate() / 1000000.0) << ','
                << setprecision(8) << fixed << maxSlowness << endl;
        lastSlowness.pop_front();
    }
    // Now record the maximum slowness among this app and the apps that ended after "start"
    double maxSlowness = slowness;
    for (std::list<std::pair<Time, double> >::iterator it = lastSlowness.begin(); it != lastSlowness.end(); ++it)
        if (maxSlowness < it->second) maxSlowness = it->second;
    lastSlowness.push_back(make_pair(end, maxSlowness));
}


void JobStatistics::beforeEvent(const Simulator::Event & ev) {
    if (typeid(*ev.msg) == typeid(AppFinishedMsg)) {
        // Finish app instance
        long int appId = boost::static_pointer_cast<AppFinishedMsg>(ev.msg)->getAppId();
        finishApp(ev.to, appId, ev.creationTime, 0);
        sim.getCurrentNode().getDatabase().appInstanceFinished(appId);
    }
}


struct UnfinishedApp {
    uint32_t node;
    long int appid;
    Time end;
    unsigned int finishedTasks;
    UnfinishedApp(uint32_t n, long int a, Time t, unsigned int f) : node(n), appid(a), end(t), finishedTasks(f) {}
    bool operator<(const UnfinishedApp & r) const { return end < r.end; }
};


JobStatistics::~JobStatistics() {
    Time now = sim.getCurrentTime();

    // Unfinished jobs
    // Calculate expected end time and remaining tasks of each application in course
    std::vector<std::map<long int, std::pair<Time, int> > > unfinishedApps(sim.getNumNodes());
    boost::shared_ptr<PerfectScheduler> ps = sim.getPerfectScheduler();
    if (ps.get()) {
        for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
            // Get the task queue
            const list<PerfectScheduler::TaskDesc> & tasks = ps->getQueue(n);
            Time end = now;
            // For each task...
            for (list<PerfectScheduler::TaskDesc>::const_iterator t = tasks.begin(); t != tasks.end(); t++) {
                // Get its app, add a finished task and check its finish time
                end += t->a;
                uint32_t origin = t->msg->getRequester().getIPNum();
                SimAppDatabase & sdb = sim.getNode(origin).getDatabase();
                long int appId = sdb.getAppId(t->msg->getRequestId());
                if (appId >= 0) {
                    std::map<long int, std::pair<Time, int> >::iterator j = unfinishedApps[origin].find(appId);
                    if (j == unfinishedApps[origin].end())
                        unfinishedApps[origin][appId] = make_pair(end, 1);
                    else {
                        if (j->second.first < end) j->second.first = end;
                        ++(j->second.second);
                    }
                }
            }
        }
    } else {
        for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
            // Get the task queue
            list<boost::shared_ptr<Task> > & tasks = sim.getNode(n).getScheduler().getTasks();
            Time end = now;
            // For each task...
            for (list<boost::shared_ptr<Task> >::iterator t = tasks.begin(); t != tasks.end(); t++) {
                // Get its app, add a finished task and check its finish time
                end += (*t)->getEstimatedDuration();
                uint32_t origin = (*t)->getOwner().getIPNum();
                SimAppDatabase & sdb = sim.getNode(origin).getDatabase();
                long int appId = sdb.getAppId((*t)->getClientRequestId());
                if (appId >= 0) {
                    std::map<long int, std::pair<Time, int> >::iterator j = unfinishedApps[origin].find(appId);
                    if (j == unfinishedApps[origin].end())
                        unfinishedApps[origin][appId] = make_pair(end, 1);
                    else {
                        if (j->second.first < end) j->second.first = end;
                        ++(j->second.second);
                    }
                }
            }
        }
    }
    // Take into account perfect scheduler queues

    // Reorder unfinished apps
    std::list<UnfinishedApp> sortedApps;
    for (unsigned int n = 0; n < sim.getNumNodes(); ++n) {
        for (std::map<long int, std::pair<Time, int> >::iterator j = unfinishedApps[n].begin(); j != unfinishedApps[n].end(); j++) {
            sortedApps.push_back(UnfinishedApp(n, j->first, j->second.first, j->second.second));
        }
    }
    sortedApps.sort();

    // Write its data and requests
    for (std::list<UnfinishedApp>::iterator it = sortedApps.begin(); it != sortedApps.end(); ++it) {
        finishApp(it->node, it->appid, it->end, it->finishedTasks);
    }

    // Finish this index
    jos << endl << endl;
    // Finished percentages
    jos << totalJobs << " jobs finished at simulation end of which " << unfinishedJobs << " (" << setprecision(2) << fixed
            << ((unfinishedJobs * 100.0) / totalJobs) << "%) didn't get any task finished." << endl << endl << endl;
    jos.precision(8);
    jos << "# Finished % CDF" << endl << CDF(finishedHist) << endl << endl;
    jos << "# JTT CDF" << endl << CDF(jttHist) << endl << endl;
    jos << "# Sequential time in src CDF" << endl << CDF(seqHist) << endl << endl;
    jos << "# Speedup CDF" << endl << CDF(spupHist) << endl << endl;
    jos << "# Slowness CDF" << endl << CDF(slownessHist) << endl << endl;

    // Finish this index
    ros << endl << endl;
    ros.precision(8);
    ros << "# Number of nodes CDF" << endl << CDF(numNodesHist) << endl << endl;
    ros << "# Search time CDF" << endl << CDF(searchHist) << endl << endl;

    // Write the slowness data of the last apps
    while (!lastSlowness.empty()) {
        double maxSlowness = 0.0;
        for (std::list<std::pair<Time, double> >::iterator it = lastSlowness.begin(); it != lastSlowness.end(); ++it)
            if (maxSlowness < it->second) maxSlowness = it->second;
        sos << setprecision(3) << fixed << (lastSlowness.front().first.getRawDate() / 1000000.0) << ','
                << setprecision(8) << fixed << maxSlowness << endl;
        lastSlowness.pop_front();
    }
}
