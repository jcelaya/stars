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
#include <assert.h>
#include "CentralizedScheduler.hpp"
#include "TaskBagMsg.hpp"
#include "TaskMonitorMsg.hpp"
#include "AcceptTaskMsg.hpp"
#include "AvailabilityInformation.hpp"
#include "Time.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "FSPScheduler.hpp"
#include "ResourceNode.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost::posix_time;
using namespace stars;


REGISTER_MESSAGE(RescheduleMsg);


CentralizedScheduler::CentralizedScheduler() :
        sim(Simulator::getInstance()), queues(sim.getNumNodes()), inTraffic(0), outTraffic(0), rescheduleSequence(1) {}


bool CentralizedScheduler::blockMessage(const boost::shared_ptr<BasicMsg> & msg) {
    return false;
}


bool CentralizedScheduler::blockEvent(const Simulator::Event & ev) {
    if (typeid(*ev.msg) == typeid(TaskBagMsg)) {
        boost::shared_ptr<TaskBagMsg> msg = static_pointer_cast<TaskBagMsg>(ev.msg);
        if (!msg->isForEN()) {
            sim.getPerfStats().startEvent("Centralized scheduling");
            inTraffic += ev.size;
            Logger::msg("Dsp.Cent", INFO, "Request ", msg->getRequestId(), " at ", ev.t,
                    " with ", msg->getLastTask() - msg->getFirstTask() + 1, " tasks of length ", msg->getMinRequirements().getLength());
            newApp(msg);
            sim.getPerfStats().endEvent("Centralized scheduling");
            // Block this message so it does not arrive to the Dispatcher
            return true;
        } else {
            // This message was sent by the centralized scheduler
            outTraffic += ev.size;
        }
    } else if (typeid(*ev.msg) == typeid(TaskMonitorMsg) && !ev.inRecvQueue
            && static_pointer_cast<TaskMonitorMsg>(ev.msg)->getTaskState(0) == Task::Finished) {
        inTraffic += ev.size;
        taskFinished(ev.from);
    }
    return false;
}


void CentralizedScheduler::taskFinished(unsigned int node) {
    Logger::msg("Dsp.Cent", INFO, "Finished a task in node ", AddrIO(node));
    list<TaskDesc> & queue = queues[node];
    if (!queue.empty()) {
        queue.pop_front();
    } else {
        Logger::msg("Dsp.Cent", ERROR, "Error: empty queue at ", AddrIO(node));
    }
}


boost::shared_ptr<RescheduleMsg> CentralizedScheduler::getRescheduleMsg(list<TaskDesc> & queue) {
    boost::shared_ptr<TaskBagMsg> lastTaskMsg = queue.back().msg;
    unsigned int numTasks = 0;
    for (auto i = queue.rbegin(); i != queue.rend() && i->msg == lastTaskMsg; ++i) {
        ++numTasks;
    }
    Logger::msg("Dsp.Cent", DEBUG, "Sending reschedule with ", numTasks, " new tasks");
    boost::shared_ptr<RescheduleMsg> rsch(new RescheduleMsg(*lastTaskMsg));
    rsch->setFromEN(false);
    rsch->setForEN(true);
    rsch->setFirstTask(queue.back().tid - numTasks + 1);
    rsch->setLastTask(queue.back().tid);
    rsch->setSeqNumber(rescheduleSequence++);
    return rsch;
}


void CentralizedScheduler::sortQueue(unsigned int node) {
    list<TaskDesc> & queue = queues[node];
    boost::shared_ptr<RescheduleMsg> rsch = getRescheduleMsg(queue);
    queue.sort();
    rsch->setSequenceLength(queue.size());
    for (auto & i : queue) {
        Logger::msg("Dsp.Cent", DEBUG, "Order: ", i.msg->getRequestId(), ", ", i.tid);
        rsch->addTask(i.msg->getRequester(), i.msg->getRequestId(), i.tid);
    }
    sim.sendMessage(sim.getNode(node).getLeaf().getFatherAddress().getIPNum(), node, rsch);
    queue.front().running = true;
    for (auto t = ++queue.begin(); t != queue.end(); ++t) {
        t->running = false;
    }
}


void CentralizedScheduler::showStatistics() {
    Logger::msg("Sim.Progress", 0, "Centralized request traffic: ", inTraffic, "B in, ", outTraffic, "B out");
}


class BlindScheduler : public CentralizedScheduler {
    bool blockMessage(const boost::shared_ptr<BasicMsg> & msg) {
        if (boost::dynamic_pointer_cast<AvailabilityInformation>(msg).get())
            return true;
        return false;
    }

    void newApp(boost::shared_ptr<TaskBagMsg> msg) {
        for (unsigned int i = msg->getFirstTask(); i <= msg->getLastTask(); ++i) {
            unsigned int n = clientVar();
            boost::shared_ptr<TaskBagMsg> tbm(msg->clone());
            tbm->setFromEN(false);
            tbm->setForEN(true);
            tbm->setFirstTask(i);
            tbm->setLastTask(i);
            sim.sendMessage(sim.getNode(n).getLeaf().getFatherAddress().getIPNum(), n, tbm);
        }
    }

    void taskFinished(unsigned int node) {
        Logger::msg("Dsp.Cent", INFO, "Finished a task in node ", AddrIO(node));
    }

    DiscreteUniformVariable clientVar;

public:
    BlindScheduler() : CentralizedScheduler(), clientVar(0, Simulator::getInstance().getNumNodes() - 1) {}
};


class CentralizedIBP : public CentralizedScheduler {
    struct NodeAvail {
        unsigned int n;
        unsigned long int a;

        static const uint32_t ALPHA_MEM = 10;
        static const uint32_t ALPHA_DISK = 1;

        bool operator<(const NodeAvail & r) { return a < r.a; }
    };

    void newApp(boost::shared_ptr<TaskBagMsg> msg) {
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
        task.d = sim.getCurrentTime();
        if (numTasks > usableNodes.size()) {
            numTasks = usableNodes.size();
        }
        for (task.tid = 1; task.tid <= numTasks; task.tid++) {
            Logger::msg("Dsp.Cent", DEBUG, "Allocating task ", task.tid);
            // Send each task to a node which can execute it
            unsigned int node = usableNodes[task.tid - 1].n;
            Logger::msg("Dsp.Cent", DEBUG, "Task allocated to node ", node, " with availability ", usableNodes[task.tid - 1].a);
            task.a = Duration(a / sim.getNode(node).getAveragePower());
            queues[node].push_back(task);
            sortQueue(node);
        }
    }
};


class CentralizedMMP : public CentralizedScheduler {
    struct QueueTime {
        unsigned int node;
        Time qTime;
        bool operator<(const QueueTime & r) const { return qTime > r.qTime; }
    };

    fs::ofstream queueos;
    Time maxQueue;
    std::vector<Time> queueEnds;

    void newApp(boost::shared_ptr<TaskBagMsg> msg) {
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

        map<unsigned int, unsigned int> tasksPerNode;
        for (unsigned int i = 0; i < numTasks; ++i) {
            pop_heap(queueCache.begin(), queueCache.end());
            QueueTime & best = queueCache.back();
            ++tasksPerNode[best.node];
            best.qTime += taskTime[best.node];
            push_heap(queueCache.begin(), queueCache.end());
        }

        TaskDesc task(msg);
        task.d = sim.getCurrentTime();
        task.tid = 1;
        for (auto i = tasksPerNode.begin(); i != tasksPerNode.end(); ++i) {
            Logger::msg("Dsp.Cent", DEBUG, "Tasks ", task.tid, " to ", task.tid + numTasks - 1, " allocated to node ", i->first);
            task.a = taskTime[i->first];
            for (unsigned int j = 0; j < i->second; ++j, ++task.tid)
                queues[i->first].push_back(task);
            sortQueue(i->first);
            updateQueueLengths(i->first, task.a * i->second);
        }
    }

    void updateQueueLengths(unsigned int node, Duration a) {
        Time now = sim.getCurrentTime();
        if (queueEnds[node] < now)
            queueEnds[node] = now;
        queueEnds[node] += a;
        if (maxQueue < queueEnds[node]) {
            queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << endl;
            maxQueue = queueEnds[node];
            queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << endl;
        }
    }

public:
    CentralizedMMP() : CentralizedScheduler(), maxQueue(sim.getCurrentTime()), queueEnds(sim.getNumNodes(), maxQueue) {
        queueos.open(sim.getResultDir() / fs::path("cent_queue_length.stat"));
        queueos << "# Time, max" << endl << setprecision(3) << fixed;
    }

    ~CentralizedMMP() {
        Time now = sim.getCurrentTime();
        queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << endl;
        queueos.close();
    }
};

class CentralizedDP : public CentralizedScheduler {
    struct Hole {
        unsigned int node;
        unsigned int numTasks;
        unsigned long remaining;
        bool operator<(const Hole & r) const { return remaining < r.remaining || (remaining == r.remaining && numTasks < r.numTasks); }
    };

    void newApp(boost::shared_ptr<TaskBagMsg> msg) {
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
                    if (node.getSch().getTasks().empty())
                        start += queue.front().a;
                    else
                        start += node.getSch().getTasks().front()->getEstimatedDuration();
                    for (list<TaskDesc>::iterator it = ++queue.begin(); it != queue.end() && it->d <= deadline; it++) {
                        start += it->a;
                    }
                }
                unsigned long avail, availTotal;
                if (queue.empty() || queue.back().d <= deadline) {
                    avail = deadline > start ? ((deadline - start).seconds() * node.getAveragePower()) : 0;
                    //availTotal = -1;
                    availTotal = avail;
                } else {
                    Time end = queue.back().d;
                    for (list<TaskDesc>::reverse_iterator it = queue.rbegin(); it != queue.rend() && it->d > deadline; it++) {
                        if (it->d < end) end = it->d;
                        end -= it->a;
                    }
                    availTotal = ((end - start).seconds() * node.getAveragePower());
                    if (deadline < end) end = deadline;
                    avail = end > start ? ((end - start).seconds() * node.getAveragePower()) : 0;
                }
                Logger::msg("Dsp.Cent", DEBUG, "Node ", n, " provides ", avail);
                // If hole is enough add it
                if (avail > a) {
                    Hole h;
                    h.node = n;
                    h.numTasks = avail / a;
                    h.remaining = availTotal - a * h.numTasks;
                    //h.remaining = avail - a * h.numTasks;
                    // Check whether it's needed
                    if (cachedTasks == 0 || cachedTasks < numTasks) {
                        // Just add the hole
                        holeCache[lastHole++] = h;
                        cachedTasks += h.numTasks;
                        push_heap(holeCache, holeCache + lastHole);
                        Logger::msg("Dsp.Cent", DEBUG, h.numTasks, " tasks can be held, and ", h.remaining, " remains");
                    } else if (h < holeCache[0]) {
                        // Add the hole but, eliminate the worst?
                        while (cachedTasks > 0 && cachedTasks - holeCache[0].numTasks + h.numTasks >= numTasks && h < holeCache[0]) {
                            cachedTasks -= holeCache[0].numTasks;
                            pop_heap(holeCache, holeCache + lastHole--);
                        }
                        holeCache[lastHole++] = h;
                        cachedTasks += h.numTasks;
                        push_heap(holeCache, holeCache + lastHole);
                        Logger::msg("Dsp.Cent", DEBUG, h.numTasks, " tasks can be held, and ", h.remaining, " remains");
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
                Logger::msg("Dsp.Cent", DEBUG, numTasks, " tasks allocated to node ", best.node, " with room for ", best.numTasks,
                        " tasks and still remains ", best.remaining);
                for (unsigned int j = 0; j < numTasks; ++j, ++task.tid)
                    queues[best.node].push_back(task);
                // Sort the queue
                sortQueue(best.node);
                ignoreTasks = 0;
            }
            lastHole--;
        }
    }
};


class CentralizedFSP : public CentralizedScheduler {
    struct SlownessTasks {
        double slowness;
        int numTasks;
        unsigned int node;
        void set(double s, int n, int i) { slowness = s; numTasks = n; node = i; }
        bool operator<(const SlownessTasks & o) const { return slowness < o.slowness || (slowness == o.slowness && numTasks < o.numTasks); }
    };

    vector<FSPTaskList> proxysN;
    vector<Time> firstTaskEndTimeN;

    virtual void taskFinished(unsigned int node) {
        FSPTaskList & proxys = proxysN[node];
        // Remove the proxy, it is set as dirty
        proxys.removeTask(proxys.front().id);
        if (!proxys.empty()) {
            // Adjust the time of the first task
            firstTaskEndTimeN[node] = Time::getCurrentTime() + (++queues[node].begin())->a;
        }
        CentralizedScheduler::taskFinished(node);
    }

    void newApp(boost::shared_ptr<TaskBagMsg> msg) {
        unsigned long a = msg->getMinRequirements().getLength();
        Time now = sim.getCurrentTime();

        // Number of tasks sent to each node
        vector<int> tpn = calculateTasksPerNode(msg->getMinRequirements(), msg->getLastTask() - msg->getFirstTask() + 1);

        // Assign tasks
        TaskDesc task(msg);
        task.tid = 1;
        double maxSlowness = 0.0;
        for (size_t n = 0; n < queues.size(); ++n) {
            if (tpn[n] > 0) {
                unsigned int tasksToSend = tpn[n];
                FSPTaskList & proxys = proxysN[n];
                proxys.addTasks(TaskProxy(a, sim.getNode(n).getAveragePower(), now), tasksToSend);
                proxys.sortMinSlowness();
                double slowness = proxys.getSlowness();
                if (maxSlowness < slowness)
                    maxSlowness = slowness;

                // Send tasks to the node
                task.d = now + Duration(slowness * a);
                task.a = Duration(a / sim.getNode(n).getAveragePower());
                if (queues[n].empty()) {
                    firstTaskEndTimeN[n] = now + task.a;
                } else {
                    // Update all deadlines
                    for (auto it = queues[n].begin(); it != queues[n].end(); ++it) {
                        it->d = it->r + Duration(slowness * it->msg->getMinRequirements().getLength());
                    }
                }

                for (unsigned int j = 0; j < tasksToSend; ++j, ++task.tid)
                    queues[n].push_back(task);
                sortQueue(n);
            }
        }
        Logger::msg("Dsp.Cent", WARN, "Application ", SimAppDatabase::getAppId(msg->getRequestId()),
                ",", msg->getRequester(), " got slowness ", maxSlowness);
    }

    vector<int> calculateTasksPerNode(const TaskDescription & req, unsigned int numTasks) {
        vector<int> tpn = initTasksPerNode(req);
        vector<std::pair<double, size_t> > slownessHeap;
        bool tryOneMoreTask = true;
        unsigned int totalTasks = 0;
        Time now = sim.getCurrentTime();
        for (int currentTpn = 1; tryOneMoreTask; ++currentTpn) {
            // Try with one more task per node
            tryOneMoreTask = false;
            for (size_t n = 0; n < queues.size(); ++n) {
                // With those functions that got an additional task per node in the last iteration,
                if (tpn[n] == currentTpn - 1) {
                    // calculate the slowness with one task more
                    FSPTaskList proxys = proxysN[n];
                    proxys.addTasks(TaskProxy(req.getLength(), sim.getNode(n).getAveragePower(), now), currentTpn);
                    proxys.sortMinSlowness();
                    double slowness = proxys.getSlowness();
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
        return tpn;
    }

    vector<int> initTasksPerNode(const TaskDescription & req) {
        Time now = sim.getCurrentTime();
        // Number of tasks sent to each node
        vector<int> tpn(queues.size(), -1);
        // Discard the uncapable nodes
        for (size_t n = 0; n < tpn.size(); ++n) {
            assert(proxysN[n].size() == queues[n].size());
            StarsNode & node = sim.getNode(n);
            if (node.getAvailableMemory() >= req.getMaxMemory() && node.getAvailableDisk() >= req.getMaxDisk()) {
                tpn[n] = 0;
                // Adjust the time of the first task
                if (!proxysN[n].empty()) {
                    proxysN[n].front().t = (firstTaskEndTimeN[n] - now).seconds();
                    if (proxysN[n].front().t < -100.0) {
                        std::cerr << "Negative time to finish (" << proxysN[n].front().t << ") for node " << n << " at " << now << endl;
                        assert(false);
                    }
                }
            }
        }
        return tpn;
    }

public:
    CentralizedFSP() : CentralizedScheduler(), proxysN(queues.size()), firstTaskEndTimeN(queues.size()) {}
};


boost::shared_ptr<CentralizedScheduler> CentralizedScheduler::createScheduler(const string & type) {
    if (type == "blind") {
        return boost::shared_ptr<CentralizedScheduler>(new BlindScheduler());
    } else if (type == "IBP") {
        return boost::shared_ptr<CentralizedScheduler>(new CentralizedIBP());
    } else if (type == "MMP") {
        return boost::shared_ptr<CentralizedScheduler>(new CentralizedMMP());
    } else if (type == "DP") {
        return boost::shared_ptr<CentralizedScheduler>(new CentralizedDP());
    } else if (type == "FSP") {
        return boost::shared_ptr<CentralizedScheduler>(new CentralizedFSP());
    } else {
        return boost::shared_ptr<CentralizedScheduler>();
    }
}
