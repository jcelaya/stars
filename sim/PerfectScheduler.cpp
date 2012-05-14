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

#include <vector>
#include <list>
#include <algorithm>
#include "PerfectScheduler.hpp"
#include "RescheduleTimer.hpp"
#include "AvailabilityInformation.hpp"
#include "TaskBagMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "AcceptTaskMsg.hpp"
#include "Time.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "ResourceNode.hpp"
#include "AppFinishedMsg.hpp"
using namespace std;
using namespace boost;
using namespace boost::posix_time;


PerfectScheduler::PerfectScheduler() : Simulator::InterEventHandler(), maxQueue(sim.getCurrentTime()), inTraffic(0), outTraffic(0) {
    queues.resize(sim.getNumNodes());
    queueEnds.resize(sim.getNumNodes(), maxQueue);
    os.open(sim.getResultDir() / fs::path("perfect_queue_length.stat"));
    os << "# Time, max" << endl << setprecision(3) << fixed;
}


bool PerfectScheduler::blockMessage(uint32_t src, uint32_t dst, const shared_ptr<BasicMsg> & msg) {
    // Block AvailabilityInformation, RescheduleTimer and AcceptTaskMsg
    if (typeid(*msg) == typeid(RescheduleTimer) || dynamic_pointer_cast<AvailabilityInformation>(msg).get() || typeid(*msg) == typeid(AcceptTaskMsg))
        return true;
    // Ignore fail tolerance
    if (msg->getName() == "HeartbeatTimeout") return true;
    else if (msg->getName() == "MonitorTimer") return true;

    return false;
}


bool PerfectScheduler::blockEvent(const Simulator::Event & ev) {
    if (typeid(*ev.msg) == typeid(TaskBagMsg)) {
        shared_ptr<TaskBagMsg> msg = static_pointer_cast<TaskBagMsg>(ev.msg);
        if (!msg->isForEN()) {
            sim.getPStats().startEvent("Perfect scheduling");

            // Traffic
            inTraffic += ev.size;

            LogMsg("Dsp.Perf", INFO) << "Request " << msg->getRequestId() << " at " << ev.t
            << " with " << (msg->getLastTask() - msg->getFirstTask() + 1) << " tasks of length " << msg->getMinRequirements().getLength();

            // Reschedule
            newApp(msg);

            sim.getPStats().endEvent("Perfect scheduling");

            // Block this message so it does not arrive to the Dispatcher
            return true;
        } else {
            // This message was sent by the centralized scheduler
            outTraffic += ev.size;
        }
    }
    return false;
}


void PerfectScheduler::afterEvent(const Simulator::Event & ev) {
    if (typeid(*ev.msg) == typeid(TaskStateChgMsg)) {
        // Meassure traffic
        inTraffic += ev.size;

        // When a task finishes, send a new one
        unsigned int n = ev.from;
        taskFinished(n);
    }
}


void PerfectScheduler::sendOneTask(unsigned int to) {
    // Pre: !queues[to].empty()
    shared_ptr<TaskBagMsg> tbm(queues[to].front().msg->clone());
    tbm->setFromEN(false);
    tbm->setForEN(true);
    tbm->setFirstTask(queues[to].front().tid);
    tbm->setLastTask(queues[to].front().tid);
    queues[to].front().running = true;
    LogMsg("Dsp.Perf", INFO) << "Finally sending a task of request" << tbm->getRequestId() << " to " << AddrIO(to) << ": " << *tbm;
    sim.sendMessage(sim.getNode(to).getE().getFather().getIPNum(), to, tbm);
}


void PerfectScheduler::taskFinished(unsigned int node) {
    queues[node].pop_front();
    if (!queues[node].empty())
        sendOneTask(node);
}


void PerfectScheduler::addToQueue(const TaskDesc & task, unsigned int node) {
    Time now = sim.getCurrentTime();
    list<TaskDesc> & queue = queues[node];
    queue.push_back(task);
    if (queue.size() == 1) {
        shared_ptr<TaskBagMsg> tbm(task.msg->clone());
        tbm->setFromEN(false);
        tbm->setForEN(true);
        tbm->setFirstTask(task.tid);
        tbm->setLastTask(task.tid);
        queue.front().running = true;
        LogMsg("Dsp.Perf", INFO) << "Finally sending a task of request" << tbm->getRequestId() << " to " << AddrIO(node) << ": " << *tbm;
        sim.sendMessage(sim.getNode(node).getE().getFather().getIPNum(), node, tbm);
    }
    if (queueEnds[node] < now)
        queueEnds[node] = now;
    queueEnds[node] += task.a;
    if (maxQueue < queueEnds[node]) {
        os << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << endl;
        maxQueue = queueEnds[node];
        os << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << endl;
    }
    shared_ptr<AcceptTaskMsg> atm(new AcceptTaskMsg);
    atm->setRequestId(task.msg->getRequestId());
    atm->setFirstTask(task.tid);
    atm->setLastTask(task.tid);
    atm->setHeartbeat(ConfigurationManager::getInstance().getHeartbeat());
    sim.injectMessage(node, task.msg->getRequester().getIPNum(), atm, Duration(), true);
}


PerfectScheduler::~PerfectScheduler() {
    Time now = sim.getCurrentTime();
    os << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << endl;
    os.close();
    LogMsg("Dsp.Perf", WARN) << "Centralised scheduler would consume (just with request traffic):";
    LogMsg("Dsp.Perf", WARN) << "  " << inTraffic << " in bytes, " << outTraffic << " out bytes";
}


//class CentralizedStretch : public PerfectScheduler {
// struct AppStretch {
//  unsigned int app;
//  double stretch;
//  bool operator<(const AppStretch & r) const { return stretch < r.stretch; }
// };
//
// class StretchVector : public vector<AppStretch> {
//  unsigned int node;
// public:
//  StretchVector(unsigned int n, unsigned int size) : vector<AppStretch>(size), node(n) {}
//  bool operator<(const StretchVector & r) const { return r.front().stretch > front().stretch; }
//  unsigned int getNode() const { return node; }
//
//  static bool comparePointer(const StretchVector * l, const StretchVector * r) { return *l < *r; }
// };
//
// void makeDeadlines(std::list<TaskDesc> & queue, double stretch);
// bool checkStretch(std::list<TaskDesc> & queue, double stretch);
// double calculateMinStretch(std::list<TaskDesc> & queue);
// double getPotentialMinStretch(unsigned int app, unsigned int node);
// void newApp(shared_ptr<TaskBagMsg> msg);
//};
//
//
//void CentralizedStretch::makeDeadlines(list<PerfectScheduler::TaskDesc> & queue, double stretch) {
// for (list<TaskDesc>::iterator i = queue.begin(); i != queue.end(); i++) {
//  i->d = i->r + Duration(i->msg->getMinRequirements()->getAppLength() * stretch);
// }
// queue.sort();
//}
//
//
//bool CentralizedStretch::checkStretch(list<PerfectScheduler::TaskDesc> & queue, double stretch) {
// Time start = current;
// makeDeadlines(queue, stretch);
// // For each task, check whether it is going to finish in time or not.
// for (list<TaskDesc>::iterator i = queue.begin(); i != queue.end(); i++) {
//  if (i->d - start < i->a) {
//   // If not, fails.
//   return false;
//  } else {
//   // Increment the estimatedStart value
//   start += i->a;
//  }
// }
// return true;
//}
//
//
//double CentralizedStretch::calculateMinStretch(list<PerfectScheduler::TaskDesc> & queue) {
// Time end = current;
// double minStretch = 0.0;
// // For each task, calculate finishing time
// for (list<TaskDesc>::const_iterator i = queue.begin(); i != queue.end(); i++) {
//  end += i->a;
//  double stretch = (end - i->r).seconds() / i->msg->getMinRequirements()->getAppLength();
//  if (stretch > minStretch)
//   minStretch = stretch;
// }
// return minStretch;
//}
//
//
//double CentralizedStretch::getPotentialMinStretch(unsigned int app, unsigned int node) {
// TaskDesc task;
// task.running = false;
// // Insert the new task in the queue of this node
// task.a = Duration((double)apps[app].a / sim.getNode(node).getAveragePower());
// list<TaskDesc> queue(queues[node]);
// queue.push_back(task);
// LogMsg("Dsp.Perf", DEBUG) << "Node has " << queue.size() << " tasks, the last one lasting "
//   << task.a << "ms of an app with length " << apps[app].w;
//
// // Calculate S_i_j
// vector<double> boundaries;
// for (list<TaskDesc>::iterator i = queue.begin(); i != queue.end(); i++)
//  for (list<TaskDesc>::iterator j = i; j != queue.end(); j++)
//   if (apps[i->app].w != apps[j->app].w) {
//    double s = (apps[j->app].r - apps[i->app].r).seconds() / (apps[i->app].w - apps[j->app].w);
//    if (s > 0) {
//     boundaries.push_back(s);
//    }
//   }
// sort(boundaries.begin(), boundaries.end());
//
// // Calculate interval by binary search
// unsigned int minStretch = 0;
// unsigned int maxStretch = boundaries.size() - 1;
// if (boundaries.empty()) {
//  makeDeadlines(queue, 1.0);
// } else if (!checkStretch(queue, boundaries[maxStretch])) {
//  makeDeadlines(queue, 2.0 * boundaries[maxStretch]);
// } else if (checkStretch(queue, boundaries[minStretch])) {
//  makeDeadlines(queue, boundaries[minStretch] / 2.0);
// } else {
//  while (maxStretch - minStretch > 1) {
//   unsigned int medStretch = (minStretch + maxStretch) / 2;
//   if (checkStretch(queue, medStretch)) maxStretch = medStretch;
//   else minStretch = medStretch;
//  }
//  makeDeadlines(queue, (boundaries[minStretch] + boundaries[maxStretch]) / 2.0);
// }
// return calculateMinStretch(queue);
//}
//
//
//void CentralizedStretch::newApp(shared_ptr<TaskBagMsg> msg) {
// unsigned int numNodes = sim.getNumNodes();
// vector<StretchVector *> stretchCache(numNodes);
// vector<unsigned int> remainingTasks(apps.size());
// unsigned int totalTasks = 0;
// for (unsigned int a = 0; a < remainingTasks.size(); a++) {
//  remainingTasks[a] = apps[a].remainingTasks;
//  totalTasks += remainingTasks[a];
// }
// // Initialize queues with running tasks and calculate stretch for first potential task
// for (unsigned int n = 0; n < numNodes; n++) {
//  queues[n].clear();
//  // Get running task, if any
//  list<shared_ptr<Task> > & tasks = sim.getNode(n).getScheduler().getTasks();
//  if (!tasks.empty()) {
//   TaskDesc task;
//   task.app = tasks.front()->getClientRequestId();
//   task.a = tasks.front()->getEstimatedDuration();
//   task.running = true;
//   queues[n].push_back(task);
//  }
//  stretchCache[n] = new StretchVector(n, apps.size());
//  StretchVector & sv = *stretchCache[n];
//  for (unsigned int a = 0; a < apps.size(); a++) {
//   sv[a].app = a;
//   if (remainingTasks[a] == 0) sv[a].stretch = 100000000000.0;
//   else sv[a].stretch = getPotentialMinStretch(a, n);
//  }
//  // Sort the values for this node
//  sort(sv.begin(), sv.end());
// }
//
// //Now order the stretch vector
// make_heap(stretchCache.begin(), stretchCache.end(), StretchVector::comparePointer);
//
// for (unsigned int t = 0; t < totalTasks; t++) {
//  LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << t;
//  // Send each task where the stretch is minimum
//  // Get node with better stretch
//  while (true) {
//   pop_heap(stretchCache.begin(), stretchCache.end(), StretchVector::comparePointer);
//   StretchVector & best = *stretchCache.back();
//   if (remainingTasks[best.front().app] > 0) break;
//   best.front().stretch = 100000000000.0;
//   sort(best.begin(), best.end());
//   // Push it again in the heap
//   push_heap(stretchCache.begin(), stretchCache.end(), StretchVector::comparePointer);
//  }
//  StretchVector & best = *stretchCache.back();
//  unsigned int bestNode = best.getNode();
//  unsigned int bestApp = best.front().app;
//  double bestStretch = best.front().stretch;
//  // Add the new task to the queue of that node
//  LogMsg("Dsp.Perf", DEBUG) << "Task of app " << bestApp << " allocated to node "
//   << bestNode << " with stretch " << bestStretch;
//  TaskDesc task;
//  task.running = false;
//  task.app = bestApp;
//  task.a = Duration((double)apps[task.app].a / sim.getNode(bestNode).getAveragePower());
//  queues[bestNode].push_back(task);
//  makeDeadlines(queues[bestNode], bestStretch);
//  remainingTasks[bestApp]--;
//
//  // Recalculate stretch for all the applications with that node
//  for (unsigned int a = 0; a < apps.size(); a++) {
//   unsigned int app = best[a].app;
//   if (remainingTasks[app] == 0) best[a].stretch = 100000000000.0;
//   else best[a].stretch = getPotentialMinStretch(app, bestNode);
//  }
//  sort(best.begin(), best.end());
//
//  // Push it again in the heap
//  push_heap(stretchCache.begin(), stretchCache.end(), StretchVector::comparePointer);
// }
//
// for (vector<StretchVector *>::iterator it = stretchCache.begin(); it != stretchCache.end(); it++) delete *it;
//}


class CentralizedRandom : public PerfectScheduler {
    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = sim.getNumNodes();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;

        TaskDesc task(msg);
        for (task.tid = 1; task.tid <= numTasks; task.tid++) {
            LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
            // Send each task to a random node
            unsigned int n = Simulator::uniform(0, numNodes - 1);
            if (queues[n].empty()) {
                PeerCompNode & node = sim.getNode(n);
                LogMsg("Dsp.Perf", DEBUG) << "Task allocated to node " << n;
                task.a = Duration(a / node.getAveragePower());
                addToQueue(task, n);
                sendOneTask(n);
            }
        }
    }
};


class CentralizedRandomSimple : public PerfectScheduler {
    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = sim.getNumNodes();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;
        //unsigned int mem = apps[appNum].req->getMaxMemory();
        //unsigned int disk = apps[appNum].req->getMaxDisk();
        // Get the nodes that can execute this app and shuffle them
        vector<unsigned int> usableNodes;
        usableNodes.reserve(numNodes);
        for (unsigned int n = 0; n < numNodes; n++) {
            //PeerCompNode & node = sim.getNode(n);
            //if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk && queues[n].empty()) {
            if (queues[n].empty()) {
                usableNodes.push_back(n);
                // Exchange with an element at random (maybe itself too)
                unsigned int pos = Simulator::uniform(0, usableNodes.size() - 1);
                usableNodes.back() = usableNodes[pos];
                usableNodes[pos] = n;
            }
        }
        TaskDesc task(msg);
        for (task.tid = 1; task.tid <= numTasks && task.tid <= usableNodes.size(); task.tid++) {
            LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
            // Send each task to a random node which can execute it
            unsigned int node = usableNodes[task.tid - 1];
            LogMsg("Dsp.Perf", DEBUG) << "Task allocated to node " << node;
            task.a = Duration(a / sim.getNode(node).getAveragePower());
            addToQueue(task, node);
        }
    }
};


class CentralizedRandomFCFS : public PerfectScheduler {
    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = sim.getNumNodes();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;
        unsigned int mem = msg->getMinRequirements().getMaxMemory();
        unsigned int disk = msg->getMinRequirements().getMaxDisk();

        // Get the nodes that can execute this app
        vector<unsigned int> usableNodes;
        usableNodes.reserve(numNodes);
        for (unsigned int n = 0; n < numNodes; n++) {
            PeerCompNode & node = sim.getNode(n);
            if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk) {
                usableNodes.push_back(n);
            }
        }

        TaskDesc task(msg);
        for (task.tid = 1; task.tid <= numTasks; task.tid++) {
            LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
            // Send each task to a random node which can execute it
            unsigned int node = usableNodes[Simulator::uniform(0, usableNodes.size() - 1)];
            LogMsg("Dsp.Perf", DEBUG) << "Task allocated to node " << node;
            task.a = Duration(a / sim.getNode(node).getAveragePower());
            addToQueue(task, node);
        }
    }
};


class CentralizedRandomDeadlines : public PerfectScheduler {
    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = sim.getNumNodes();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;
        unsigned int mem = msg->getMinRequirements().getMaxMemory();
        unsigned int disk = msg->getMinRequirements().getMaxDisk();
        Time deadline = msg->getMinRequirements().getDeadline();

        // Get the nodes that can execute this app
        vector<unsigned int> usableNodes;
        usableNodes.reserve(numNodes);
        for (unsigned int n = 0; n < numNodes; n++) {
            PeerCompNode & node = sim.getNode(n);
            if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk) {
                usableNodes.push_back(n);
            }
        }

        TaskDesc task(msg);
        task.d = deadline;
        for (task.tid = 1; task.tid <= numTasks; task.tid++) {
            //LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
            // Send each task to a random node which can execute it
            unsigned int n = usableNodes[Simulator::uniform(0, usableNodes.size() - 1)];
            // Calculate ent time for all the tasks
            PeerCompNode & node = sim.getNode(n);
            task.a = Duration(a / node.getAveragePower());
            list<TaskDesc> & queue = queues[n];
            Time start = sim.getCurrentTime() + Duration(1.0); // 1 second for the msg sending
            bool meets = true;
            if (!queue.empty()) {
                list<TaskDesc>::iterator it = queue.begin();
                if (node.getScheduler().getTasks().empty())
                    start += it->a;
                else
                    start += node.getScheduler().getTasks().front()->getEstimatedDuration();// + Duration(1.0);
                for (it++; it != queue.end() && it->d <= deadline; it++) {
                    start += it->a;// + Duration(1.0);
                }
                start += task.a;
                meets = start <= deadline;
                for (; meets && it != queue.end(); it++) {
                    start += it->a;// + Duration(1.0);
                    meets = start <= it->d;
                }
            } else {
                meets = start + task.a < deadline;
            }
            if (meets) {
                //LogMsg("Dsp.Perf", DEBUG) << "Task allocated to node " << n;
                addToQueue(task, n);
                queue.sort();
            }
            // If this node does not meet deadlines, drop the task
        }
    }
};


class CentralizedSimple : public PerfectScheduler {
    struct NodeAvail {
        unsigned int n;
        unsigned long int a;

        static const uint32_t ALPHA_MEM = 10;
        static const uint32_t ALPHA_DISK = 1;

        bool operator<(const NodeAvail & r) {
            return a < r.a;
        }
    };

    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = sim.getNumNodes();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;
        unsigned int mem = msg->getMinRequirements().getMaxMemory();
        unsigned int disk = msg->getMinRequirements().getMaxDisk();

        // Get the k best nodes that can execute this app and order them
        vector<NodeAvail> usableNodes;
        usableNodes.reserve(numTasks);
        for (unsigned int n = 0; n < numNodes; n++) {
            PeerCompNode & node = sim.getNode(n);
            if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk && queues[n].empty()) {
                NodeAvail avail;
                avail.n = n;
                avail.a = (node.getAvailableMemory() - mem) * NodeAvail::ALPHA_MEM + (node.getAvailableDisk() - disk) * NodeAvail::ALPHA_DISK;
                // If there are less than k nodes in the queue, or the new one is better than the worst, push it in
                if (usableNodes.size() < numTasks) {
                    usableNodes.push_back(avail);
                    push_heap(usableNodes.begin(), usableNodes.end());
                } else if (usableNodes.front().a > avail.a) {
                    pop_heap(usableNodes.begin(), usableNodes.end());
                    usableNodes.back() = avail;
                    push_heap(usableNodes.begin(), usableNodes.end());
                }
            }
        }

        TaskDesc task(msg);
        if (numTasks > usableNodes.size()) numTasks = usableNodes.size();
        for (task.tid = 1; task.tid <= numTasks; task.tid++) {
            LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
            // Send each task to a random node which can execute it
            unsigned int node = usableNodes[task.tid - 1].n;
            LogMsg("Dsp.Perf", DEBUG) << "Task allocated to node " << node << " with availability " << usableNodes[task.tid - 1].a;
            task.a = Duration(a / sim.getNode(node).getAveragePower());
            addToQueue(task, node);
        }
    }
};


class CentralizedFCFS : public PerfectScheduler {
    struct QueueTime {
        unsigned int node;
        Time qTime;
        bool operator<(const QueueTime & r) const {
            return qTime > r.qTime;
        }
    };

    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = sim.getNumNodes();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;
        unsigned int mem = msg->getMinRequirements().getMaxMemory();
        unsigned int disk = msg->getMinRequirements().getMaxDisk();
        Time now = sim.getCurrentTime();

        vector<QueueTime> queueCache;
        queueCache.reserve(numNodes);
        vector<Duration> taskTime(numNodes);
        // Calculate queue time for first potential task on suitable nodes
        for (unsigned int n = 0; n < numNodes; n++) {
            PeerCompNode & node = sim.getNode(n);
            if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk) {
                QueueTime qt;
                qt.node = n;
                qt.qTime = queueEnds[n] < now ? now : queueEnds[n];
                // Add the new task
                taskTime[n] = Duration(a / node.getAveragePower());
                qt.qTime += taskTime[n];
                queueCache.push_back(qt);
            }
        }
        if (queueCache.empty()) return;

        //Now order the queue time vector
        make_heap(queueCache.begin(), queueCache.end());

        TaskDesc task(msg);
        for (task.tid = 1; task.tid <= numTasks; task.tid++) {
            LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
            // Send each task where the queue is shorter
            pop_heap(queueCache.begin(), queueCache.end());
            QueueTime & best = queueCache.back();
            LogMsg("Dsp.Perf", DEBUG) << "Task allocated to node " << best.node << " with queue time " << best.qTime;
            task.a = taskTime[best.node];
            addToQueue(task, best.node);
            best.qTime += task.a;
            push_heap(queueCache.begin(), queueCache.end());
        }
    }
};

class CentralizedDeadlines : public PerfectScheduler {
    struct Hole {
        unsigned int node;
        unsigned int numTasks;
        unsigned long remaining;
        bool operator<(const Hole & r) const {
            return remaining < r.remaining || (remaining == r.remaining && numTasks < r.numTasks);
        }
    };

    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = sim.getNumNodes();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;
        unsigned int mem = msg->getMinRequirements().getMaxMemory();
        unsigned int disk = msg->getMinRequirements().getMaxDisk();
        unsigned int cachedTasks = 0;
        Time deadline = msg->getMinRequirements().getDeadline();

        Hole holeCache[numTasks];
        unsigned int lastHole = 0;
        // Calculate holes for suitable nodes
        for (unsigned int n = 0; n < numNodes; n++) {
            PeerCompNode & node = sim.getNode(n);
            if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk) {
                list<TaskDesc> & queue = queues[n];
                // Calculate available hole for this application
                Time start = sim.getCurrentTime() + Duration(1.0); // 1 second for the msg sending
                if (!queue.empty()) {
                    if (node.getScheduler().getTasks().empty())
                        start += queue.front().a;
                    else
                        start += node.getScheduler().getTasks().front()->getEstimatedDuration();// + Duration(1.0);
                    for (list<TaskDesc>::iterator it = ++queue.begin(); it != queue.end() && it->d <= deadline; it++) {
                        start += it->a + Duration(1.0);
                    }
                }
                unsigned long avail, availTotal;
                if (queue.empty() || queue.back().d <= deadline) {
                    avail = deadline > start ? ((deadline - start).seconds() * sim.getNode(n).getAveragePower()) : 0;
                    availTotal = -1;//avail - avail % a;
                } else {
                    Time end = queue.back().d;
                    for (list<TaskDesc>::reverse_iterator it = queue.rbegin(); it != queue.rend() && it->d > deadline; it++) {
                        if (it->d < end) end = it->d;
                        end -= it->a + Duration(1.0); // 1 second for the msg sending
                    }
                    availTotal = ((end - start).seconds() * sim.getNode(n).getAveragePower());
                    if (deadline < end) end = deadline;
                    avail = end > start ? ((end - start).seconds() * sim.getNode(n).getAveragePower()) : 0;
                }
                //LogMsg("Dsp.Perf", DEBUG) << "Node " << n << " provides " << avail;
                // If hole is enough add it
                if (avail > a) {
                    Hole h;
                    h.node = n;
                    h.numTasks = avail / a;
                    h.remaining = availTotal - a * h.numTasks;
                    // Check whether it's needed
                    if (cachedTasks == 0 || cachedTasks < numTasks) {
                        // Just add the hole
                        holeCache[lastHole++] = h;
                        cachedTasks += h.numTasks;
                        push_heap(holeCache, holeCache + lastHole);
                        //LogMsg("Dsp.Perf", DEBUG) << h.numTasks << " tasks can be held, and " << h.remaining << " remains";
                    } else if (h < holeCache[0]) {
                        // Add the hole but, eliminate the worst?
                        while (cachedTasks > 0 && cachedTasks - holeCache[0].numTasks + h.numTasks >= numTasks && h < holeCache[0]) {
                            cachedTasks -= holeCache[0].numTasks;
                            pop_heap(holeCache, holeCache + lastHole--);
                        }
                        holeCache[lastHole++] = h;
                        cachedTasks += h.numTasks;
                        push_heap(holeCache, holeCache + lastHole);
                        //LogMsg("Dsp.Perf", DEBUG) << h.numTasks << " tasks can be held, and " << h.remaining << " remains";
                    }
                }
            }
        }

        unsigned int ignoreTasks = cachedTasks > numTasks ? cachedTasks - numTasks : 0;

        TaskDesc task(msg);
        task.tid = 1;
        task.d = msg->getMinRequirements().getDeadline();
        while (lastHole > 0) {
            // Send tasks to the next hole
            pop_heap(holeCache, holeCache + lastHole);
            Hole & best = holeCache[lastHole - 1];
            task.a = Duration(a / sim.getNode(best.node).getAveragePower());

            // Insert as many tasks as possible
            if (best.numTasks <= ignoreTasks) {
                ignoreTasks -= best.numTasks;
            } else {
                unsigned int numTasks = best.numTasks - ignoreTasks;
                //LogMsg("Dsp.Perf", DEBUG) << numTasks << " tasks allocated to node " << best.node << " with room for " << best.numTasks
                //  << " tasks and still remains " << best.remaining;
                for (unsigned int i = 0; i < numTasks; i++, task.tid++) {
                    //LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
                    addToQueue(task, best.node);
                }
                // Sort the queue
                queues[best.node].sort();
                ignoreTasks = 0;
            }
            lastHole--;
        }
    }
};


shared_ptr<PerfectScheduler> PerfectScheduler::createScheduler(const string & type) {
// if (type == "MScent")
//  return shared_ptr<PerfectScheduler>(new CentralizedStretch());
// else
    if (type == "Random")
        return shared_ptr<PerfectScheduler>(new CentralizedRandom());
    else if (type == "SSrandom")
        return shared_ptr<PerfectScheduler>(new CentralizedRandomSimple());
    else if (type == "FCFSrandom")
        return shared_ptr<PerfectScheduler>(new CentralizedRandomFCFS());
    else if (type == "DSrandom")
        return shared_ptr<PerfectScheduler>(new CentralizedRandomDeadlines());
    else if (type == "SScent")
        return shared_ptr<PerfectScheduler>(new CentralizedSimple());
    else if (type == "FCFScent")
        return shared_ptr<PerfectScheduler>(new CentralizedFCFS());
    else if (type == "DScent")
        return shared_ptr<PerfectScheduler>(new CentralizedDeadlines());
    return shared_ptr<PerfectScheduler>();
}
