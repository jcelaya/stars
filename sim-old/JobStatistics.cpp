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

#include <list>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "JobStatistics.hpp"
#include "SimAppDatabase.hpp"
#include "TaskMonitorMsg.hpp"
#include "TaskBagMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "CommAddress.hpp"
#include "Scheduler.hpp"
#include "Logger.hpp"
#include "PeerCompNode.hpp"
#include "AcceptTaskMsg.hpp"
#include "AppFinishedMsg.hpp"
#include "PerfectScheduler.hpp"
using namespace std;
using namespace boost::posix_time;


JobStatistics::JobStatistics() : Simulator::InterEventHandler(), numNodesHist(1.0), finishedHist(0.1), searchHist(100U),
        jttHist(100U), seqHist(100U), spupHist(0.1), stretchHist(100U), unfinishedJobs(0), totalJobs(0),
        lastTSample(sim.getCurrentTime()), partialFinishedTasks(0), totalFinishedTasks(0) {
    fs::path statDir = sim.getResultDir();

    jos.open(statDir / fs::path("apps.stat"));
    jos << "# App. ID, src node, num tasks, task size, task mem, task disk, release date, deadline, num finished, JTT, sequential time at src, stretch" << endl;
    ros.open(statDir / fs::path("requests.stat"));
    ros << "# Req. ID, App. ID, num tasks, num nodes, num accepted, release date, search time" << endl;
    tos.open(statDir / fs::path("throughput.stat"));
    tos << "# Time, tasks finished per second, total tasks finished" << endl;
    tos << "0,0,0";
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
    double stretch = 0.0;
    for (vector<SimAppDatabase::AppInstance::Task>::const_iterator it = app.tasks.begin(); it != app.tasks.end(); it++)
        if (it->state == Task::Finished) finishedTasks++;
    finishedHist.addValue((finishedTasks * 100.0) / app.req.getNumTasks());
    if (finishedTasks > 0) {
        // Only count jobs with at least one finished task
        jttHist.addValue(jtt);
        seqHist.addValue(sequential);
        double speedup = sequential * finishedTasks / app.req.getNumTasks() / jtt;
        spupHist.addValue(speedup);
        unsigned long int effAppLength = app.req.getLength() * finishedTasks;
        stretch = jtt / effAppLength;
        stretchHist.addValue(stretch);
    } else unfinishedJobs++;
    jos << appId << ',' << CommAddress(node, ConfigurationManager::getInstance().getPort()) << ','
    << app.req.getNumTasks() << ',' << app.req.getLength() << ','
    << app.req.getMaxMemory() << ',' << app.req.getMaxDisk() << ','
    << setprecision(3) << fixed << (app.ctime.getRawDate() / 1000000.0) << ','
    << (app.req.getDeadline().getRawDate() / 1000000.0) << ','
    << finishedTasks << ',' << jtt << ',' << sequential << ','
    << setprecision(8) << fixed << stretch << endl;

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
}


void JobStatistics::beforeEvent(const Simulator::Event & ev) {
    if (typeid(*ev.msg) == typeid(TaskStateChgMsg) && boost::static_pointer_cast<TaskStateChgMsg>(ev.msg)->getNewState() == Task::Finished) {
        partialFinishedTasks++;
        totalFinishedTasks++;
        Time now = sim.getCurrentTime();
        double elapsed = (now - lastTSample).seconds();
        if (elapsed >= delayTSample) {
            tos << setprecision(3) << fixed << (now.getRawDate() / 1000000.0) << ',' << (partialFinishedTasks / elapsed) << ',' << totalFinishedTasks << endl;
            partialFinishedTasks = 0;
            lastTSample = now;
        }
    } else if (typeid(*ev.msg) == typeid(AppFinishedMsg)) {
        // Finish app instance
        long int appId = boost::static_pointer_cast<AppFinishedMsg>(ev.msg)->getAppId();
        finishApp(ev.to, appId, ev.creationTime, 0);
        sim.getCurrentNode().getDatabase().appInstanceFinished(appId);
    }
}


//struct StartEndStretch {
// double start, end, length;
// double stretch;
// bool operator<(const StartEndStretch & r) { return start < r.start; }
//};


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
                SimAppDatabase & sdb = sim.getNode(t->msg->getRequester().getIPNum()).getDatabase();
                long int appId = sdb.getAppId(t->msg->getRequestId());
                if (appId >= 0) {
                    std::map<long int, std::pair<Time, int> >::iterator j = unfinishedApps[n].find(appId);
                    if (j == unfinishedApps[n].end())
                        unfinishedApps[n][appId] = make_pair(end, 1);
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
                SimAppDatabase & sdb = sim.getNode((*t)->getOwner().getIPNum()).getDatabase();
                long int appId = sdb.getAppId((*t)->getClientRequestId());
                if (appId >= 0) {
                    std::map<long int, std::pair<Time, int> >::iterator j = unfinishedApps[n].find(appId);
                    if (j == unfinishedApps[n].end())
                        unfinishedApps[n][appId] = make_pair(end, 1);
                    else {
                        if (j->second.first < end) j->second.first = end;
                        ++(j->second.second);
                    }
                }
            }
        }
    }
    // Take into account perfect scheduler queues

    // Write its data and requests
    for (unsigned int n = 0; n < sim.getNumNodes(); n++) {
        for (std::map<long int, std::pair<Time, int> >::iterator j = unfinishedApps[n].begin(); j != unfinishedApps[n].end(); j++) {
            finishApp(n, j->first, j->second.first, j->second.second);
        }
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
    jos << "# Stretch CDF" << endl << CDF(stretchHist) << endl << endl;

    // Finish this index
    ros << endl << endl;
    ros.precision(8);
    ros << "# Number of nodes CDF" << endl << CDF(numNodesHist) << endl << endl;
    ros << "# Search time CDF" << endl << CDF(searchHist) << endl << endl;

    double elapsed = (now - lastTSample).seconds();
    tos << setprecision(3) << fixed << (now.getRawDate() / 1000000.0) << ',' << (partialFinishedTasks / elapsed) << ',' << totalFinishedTasks << endl;

// // Measure max and min stretch at each app release
// // Make a list of requests in release order
// list<StartEndStretch> l;
// for (map<int64_t, Job>::const_iterator it = jobs.begin(); it != jobs.end(); it++) {
//  StartEndStretch e;
//  e.start = it->second.submission.getRawDate() / 1000000.0;
//  e.end = it->second.jobFinished.getRawDate() / 1000000.0;
//  if (e.end <= e.start) continue;   // Unfinished app
//  e.length = e.end - e.start;
//  e.stretch = e.length / it->second.minReqs->getAppLength();
//  l.push_back(e);
// }
// l.sort();
// os << "# Time, max stretch, min stretch, ratio" << endl;
// for (list<StartEndStretch>::iterator it = l.begin(); it != l.end(); it++) {
//  // delete all previous apps which ended before this one started
//  while (l.front().end <= it->start) l.pop_front();
//  // calculate max and min stretch among tasks with a minimum overlap percentage
//  double maxStretch = it->stretch, minStretch = it->stretch;
//  for (list<StartEndStretch>::iterator jt = l.begin(); jt != l.end() && jt->start < it->end; jt++) {
//   double maxStart = jt->start < it->start ? it->start : jt->start;
//   double minEnd = jt->end < it->end ? jt->end : it->end;
//   double shorterApp = it->length < jt->length ? it->length : jt->length;
//   double overlapRatio = (minEnd - maxStart) / shorterApp;
//   if (overlapRatio >= 0.5) {
//    if (jt->stretch > maxStretch) maxStretch = jt->stretch;
//    if (jt->stretch < minStretch) minStretch = jt->stretch;
//   }
//  }
//  os << it->start << ',' << maxStretch << ',' << minStretch << ',' << (maxStretch / minStretch) << endl;
// }
// os << endl << endl;
}
