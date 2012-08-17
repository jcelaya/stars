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

#include <algorithm>
#include "PerfectScheduler.hpp"
#include "RescheduleTimer.hpp"
#include "AvailabilityInformation.hpp"
#include "TaskBagMsg.hpp"
#include "TaskMonitorMsg.hpp"
#include "AcceptTaskMsg.hpp"
#include "Time.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "MinSlownessScheduler.hpp"
#include "ResourceNode.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost;
using namespace boost::posix_time;


PerfectScheduler::PerfectScheduler() : sim(Simulator::getInstance()), queues(sim.getNumNodes()),
        maxQueue(sim.getCurrentTime()), queueEnds(sim.getNumNodes(), maxQueue), inTraffic(0), outTraffic(0) {
    os.open(sim.getResultDir() / fs::path("perfect_queue_length.stat"));
    os << "# Time, max" << endl << setprecision(3) << fixed;
}


bool PerfectScheduler::blockMessage(const shared_ptr<BasicMsg> & msg) {
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
            sim.getPerfStats().startEvent("Perfect scheduling");

            // Traffic
            inTraffic += ev.size;

            LogMsg("Dsp.Perf", INFO) << "Request " << msg->getRequestId() << " at " << ev.t
                    << " with " << (msg->getLastTask() - msg->getFirstTask() + 1) << " tasks of length " << msg->getMinRequirements().getLength();

            // Reschedule
            newApp(msg);

            sim.getPerfStats().endEvent("Perfect scheduling");

            // Block this message so it does not arrive to the Dispatcher
            return true;
        } else {
            // This message was sent by the centralized scheduler
            outTraffic += ev.size;
        }
    } else if (typeid(*ev.msg) == typeid(TaskMonitorMsg)) {
        // Meassure traffic
        inTraffic += ev.size;

        // When a task finishes, send a new one
        unsigned int n = ev.from;
        taskFinished(n);
    }
    return false;
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
                StarsNode & node = sim.getNode(n);
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
            //StarsNode & node = sim.getNode(n);
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
            StarsNode & node = sim.getNode(n);
            if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk) {
                usableNodes.push_back(n);
                // Exchange with an element at random to shuffle the list (maybe itself too)
                unsigned int pos = Simulator::uniform(0, usableNodes.size() - 1);
                usableNodes.back() = usableNodes[pos];
                usableNodes[pos] = n;
            }
        }

        TaskDesc task(msg);
        for (task.tid = 1; task.tid <= numTasks; task.tid++) {
            LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
            // Send each task to a random node which can execute it
            unsigned int node = usableNodes[task.tid % usableNodes.size()];
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
            StarsNode & node = sim.getNode(n);
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
            StarsNode & node = sim.getNode(n);
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

        bool operator<(const NodeAvail & r) { return a < r.a; }
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
            StarsNode & node = sim.getNode(n);
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
        bool operator<(const QueueTime & r) const { return qTime > r.qTime; }
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
            StarsNode & node = sim.getNode(n);
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
        bool operator<(const Hole & r) const { return remaining < r.remaining || (remaining == r.remaining && numTasks < r.numTasks); }
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
            StarsNode & node = sim.getNode(n);
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


class CentralizedMinSlowness : public PerfectScheduler {
    struct SlownessTasks {
        double slowness;
        int numTasks;
        unsigned int node;
        void set(double s, int n, int i) { slowness = s; numTasks = n; node = i; }
        bool operator<(const SlownessTasks & o) const { return slowness < o.slowness || (slowness == o.slowness && numTasks < o.numTasks); }
    };

    vector<list<TaskProxy> > proxysN;
    vector<vector<std::pair<double, int> > > switchValuesN;

    virtual void taskFinished(unsigned int node) {
        list<TaskProxy> & proxys = proxysN[node];
        vector<std::pair<double, int> > & switchValues = switchValuesN[node];
        // Assume it's the first proxy
        vector<double> svOld;
        // Calculate bounds with the rest of the tasks, except the first
        for (list<TaskProxy>::iterator it = ++proxys.begin(); it != proxys.end(); ++it)
            if (it->a != proxys.front().a) {
                double l = (proxys.front().rabs - it->rabs).seconds() / (it->a - proxys.front().a);
                if (l > 0.0) {
                    svOld.push_back(l);
                }
            }
        std::sort(svOld.begin(), svOld.end());
        // Create a new list of switch values without the removed ones, counting occurrences
        vector<std::pair<double, int> > svCur(switchValues.size());
        vector<std::pair<double, int> >::iterator in1 = switchValues.begin(), out = svCur.begin();
        vector<double>::iterator in2 = svOld.begin();
        while (in1 != switchValues.end() && in2 != svOld.end()) {
            if (in1->first != *in2) *out++ = *in1++;
            else {
                if (in1->second > 1) {
                    out->first = in1->first;
                    (out++)->second = in1->second - 1;
                }
                ++in1;
                ++in2;
            }
        }
        if (in2 == svOld.end())
            out = std::copy(in1, switchValues.end(), out);
        if (out != svCur.end())
            svCur.erase(out, svCur.end());
        switchValues.swap(svCur);
        // Remove the proxy
        proxys.pop_front();

        PerfectScheduler::taskFinished(node);
    }

    double addTasks(list<TaskProxy> & proxys, vector<std::pair<double, int> > & switchValues, unsigned int n, double a, double power, Time now) {
        size_t numOriginalTasks = proxys.size();
        proxys.push_back(TaskProxy(a, power));
        list<TaskProxy>::iterator ni = --proxys.end();
        for (unsigned int j = 1; j < n; ++j)
            proxys.push_back(TaskProxy(a, power));
        vector<double> svNew;
        // If there were more than the first task, sort them with the new tasks
        if (numOriginalTasks > 1) {
            svNew.reserve(switchValues.size());
            // Calculate bounds with the rest of the tasks, except the first
            for (list<TaskProxy>::iterator it = ++proxys.begin(); it != ni; ++it)
                if (it->a != ni->a) {
                    double l = (ni->rabs - it->rabs).seconds() / (it->a - ni->a);
                    if (l > 0.0) {
                        svNew.push_back(l);
                    }
                }
            std::sort(svNew.begin(), svNew.end());
            // Create new vector of values, counting occurrences
            vector<std::pair<double, int> > svCur(switchValues.size() + svNew.size());
            vector<std::pair<double, int> >::iterator in1 = switchValues.begin(), out = svCur.begin();
            vector<double>::iterator in2 = svNew.begin();
            while (in1 != switchValues.end() && in2 != svNew.end()) {
                if (in1->first < *in2) *out++ = *in1++;
                else if (in1->first > *in2) *out++ = std::make_pair(*in2++, n);
                else {
                    out->first = in1->first;
                    (out++)->second = (in1++)->second + n;
                    ++in2;
                }
            }
            if (in1 == switchValues.end())
                while (in2 != svNew.end()) *out++ = std::make_pair(*in2++, n);
            if (in2 == svNew.end())
                out = std::copy(in1, switchValues.end(), out);
            if (out != svCur.end())
                svCur.erase(out, svCur.end());
            switchValues.swap(svCur);

            svNew.resize(switchValues.size());
            for (size_t i = 0; i < svNew.size(); ++i)
                svNew[i] = switchValues[i].first;
            TaskProxy::sortMinSlowness(proxys, svNew);
        }
        double slowness = 0.0;
        Time e = now;
        // For each task, calculate finishing time
        for (list<TaskProxy>::iterator i = proxys.begin(); i != proxys.end(); ++i) {
            e += Duration(i->t);
            double s = (e - i->rabs).seconds() / i->a;
            if (s > slowness)
                slowness = s;
        }
        return slowness;
    }

    void newApp(shared_ptr<TaskBagMsg> msg) {
        unsigned int numNodes = queues.size();
        unsigned long a = msg->getMinRequirements().getLength();
        unsigned int numTasks = msg->getLastTask() - msg->getFirstTask() + 1;
        unsigned int mem = msg->getMinRequirements().getMaxMemory();
        unsigned int disk = msg->getMinRequirements().getMaxDisk();
        Time now = sim.getCurrentTime();

        unsigned int totalTasks = 0;
        // Number of tasks sent to each node
        vector<int> tpn(numNodes);
        // Discard the uncapable nodes
        for (size_t n = 0; n < numNodes; ++n) {
            StarsNode & node = sim.getNode(n);
            tpn[n] = node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk ? 0 : -1;
        }
        // Heap of slowness values
        vector<std::pair<double, size_t> > slownessHeap;

        bool tryOneMoreTask = true;
        for (int currentTpn = 1; tryOneMoreTask; ++currentTpn) {
            // Try with one more task per node
            tryOneMoreTask = false;
            for (size_t n = 0; n < numNodes; ++n) {
                // With those functions that got an additional task per node in the last iteration,
                if (tpn[n] == currentTpn - 1) {
                    // calculate the slowness with one task more
                    list<TaskProxy> proxys = proxysN[n];
                    vector<std::pair<double, int> > switchValues = switchValuesN[n];
                    double slowness = addTasks(proxys, switchValues, currentTpn, a, sim.getNode(n).getAveragePower(), now);
                    // If the slowness is less than the maximum to date, or there aren't enough tasks yet, insert it in the heap
                    if (totalTasks < numTasks || slowness < slownessHeap.front().first) {
                        slownessHeap.push_back(std::make_pair(slowness, n));
                        std::push_heap(slownessHeap.begin(), slownessHeap.end());
                        // Add one task to that node
                        ++tpn[n];
                        ++totalTasks;
                        // Remove the highest slowness values as long as there are enough tasks
                        if (totalTasks > numTasks) {
                            --totalTasks;
                            --tpn[slownessHeap.front().second];
                            std::pop_heap(slownessHeap.begin(), slownessHeap.end());
                            slownessHeap.pop_back();
                        }
                        // As long as one function gets another task, we continue trying
                        tryOneMoreTask = true;
                    }
                }
            }
        }

        // Assign tasks
        TaskDesc task(msg);
        task.tid = 1;
        for (size_t n = 0; n < numNodes; ++n) {
            if (tpn[n] > 0) {
                unsigned int tasksToSend = tpn[n];
                list<TaskProxy> & proxys = proxysN[n];
                vector<std::pair<double, int> > & switchValues = switchValuesN[n];
                double slowness = addTasks(proxys, switchValues, tasksToSend, a, sim.getNode(n).getAveragePower(), now);

                // Send tasks to the node
                task.d = now + Duration(slowness * msg->getMinRequirements().getLength());
                task.a = Duration(a / sim.getNode(n).getAveragePower());

                //LogMsg("Dsp.Perf", DEBUG) << tasksToNode << " tasks allocated to node " << assignment[i].node << " with room for " << assignment[i].numTasks << " tasks";
                for (unsigned int j = 0; j < tasksToSend; j++, task.tid++) {
                    //LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
                    addToQueue(task, n);
                }
                // Sort the queue
                queues[n].sort();
            }
        }
    }


//		vector<SlownessTasks> assignment;
//        for (unsigned int n = 0; n < numNodes; ++n) {
//            StarsNode & node = sim.getNode(n);
//        	// Obtain the slowness of assigning one task
//            if (node.getAvailableMemory() >= mem && node.getAvailableDisk() >= disk) {
//            	// From those nodes that fulfill memory and disk requirements
//            	list<TaskProxy> proxys;
//            	getProxys(n, proxys);
//            	proxys.push_back(TaskProxy(msg->getMinRequirements().getLength(), node.getAveragePower()));
//            	vector<double> lBounds;
//            	getBounds(proxys, lBounds);
//            	TaskProxy::sortMinSlowness(proxys, lBounds);
//            	double minSlowness = 0.0;
//        		Time e = now;
//        		// For each task, calculate finishing time
//        		for (list<TaskProxy>::iterator i = proxys.begin(); i != proxys.end(); ++i) {
//        			e += Duration(i->t);
//        			double slowness = (e - i->rabs).seconds() / i->a;
//        			if (slowness > minSlowness)
//        				minSlowness = slowness;
//        		}
//        		assignment.push_back(SlownessTasks());
//            	assignment.back().set(minSlowness, 1, n);
//            }
//        }
//		unsigned int suitableNodes = assignment.size();
//        if (assignment.empty()) return;
//        sort(assignment.begin(), assignment.end());
//
//        // Calculate if it is better to assign more tasks
//        unsigned int tasksPerNode = 1;
//		while (true) {
//			// Calculate the last function index
//			unsigned int totalTasks = 0, last;
//			for (last = 0; last < suitableNodes && totalTasks < numTasks; ++last)
//				totalTasks += assignment[last].numTasks;
//			// Calculate the slowness with one more task per node
//			++tasksPerNode;
//			vector<SlownessTasks> st1Sorted(last);
//			for (size_t i = 0; i < last; ++i) {
//	            StarsNode & node = sim.getNode(assignment[i].node);
//	        	// Obtain the slowness
//            	list<TaskProxy> proxys;
//            	getProxys(assignment[i].node, proxys);
//				for (unsigned int j = 0; j < tasksPerNode; ++j)
//					proxys.push_back(TaskProxy(msg->getMinRequirements().getLength(), node.getAveragePower()));
//            	vector<double> lBounds;
//            	getBounds(proxys, lBounds);
//            	TaskProxy::sortMinSlowness(proxys, lBounds);
//            	double minSlowness = 0.0;
//        		Time e = now;
//        		// For each task, calculate finishing time
//        		for (list<TaskProxy>::iterator it = proxys.begin(); it != proxys.end(); ++it) {
//        			e += Duration(it->t);
//        			double slowness = (e - it->rabs).seconds() / it->a;
//        			if (slowness > minSlowness)
//        				minSlowness = slowness;
//        		}
//				st1Sorted[i].set(minSlowness, tasksPerNode, assignment[i].node);
//			}
//			sort(st1Sorted.begin(), st1Sorted.end());
//			// Substitute those functions that provide lower slowness with more tasks per node than the maximum
//			// If there are none, end
//			if (totalTasks >= numTasks && st1Sorted[0].slowness >= assignment[last - 1].slowness) break;
//			for (size_t i = 0; i < last && (totalTasks < numTasks || st1Sorted[i].slowness < assignment[last - 1].slowness); ++i) {
//				assignment[i] = st1Sorted[i];
//			}
//		}
//
//		// Assign tasks
//        TaskDesc task(msg);
//        task.tid = 1;
//        unsigned int totalTasks = 0;
//        for (unsigned int i = 0; i < suitableNodes && totalTasks < numTasks; ++i) {
//            // Send tasks to the next node
//            task.d = now + Duration(assignment[i].slowness * msg->getMinRequirements().getLength());
//            task.a = Duration(a / sim.getNode(assignment[i].node).getAveragePower());
//
//			unsigned int tasksToNode = assignment[i].numTasks;
//			totalTasks += tasksToNode;
//			if (totalTasks > numTasks) tasksToNode -= totalTasks - numTasks;
//			//LogMsg("Dsp.Perf", DEBUG) << tasksToNode << " tasks allocated to node " << assignment[i].node << " with room for " << assignment[i].numTasks << " tasks";
//			for (unsigned int j = 0; j < tasksToNode; j++, task.tid++) {
//				//LogMsg("Dsp.Perf", DEBUG) << "Allocating task " << task.tid;
//				addToQueue(task, assignment[i].node);
//			}
//			// Sort the queue
//			queues[assignment[i].node].sort();
//        }
//	}

public:
    CentralizedMinSlowness() : PerfectScheduler(), proxysN(queues.size()), switchValuesN(queues.size()) {}
};


shared_ptr<PerfectScheduler> PerfectScheduler::createScheduler(const string & type) {
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
    else if (type == "MScent")
        return shared_ptr<PerfectScheduler>(new CentralizedMinSlowness());
    return shared_ptr<PerfectScheduler>();
}
