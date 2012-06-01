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
 *  51 Franklin St, Fifth Floor, Boston, MA••••••••  02110-1301  USA
 *
 */

#include <sys/resource.h>
#include <unistd.h>
#include <csignal>
#include <iostream>
#include <algorithm>
#include <log4cpp/Category.hh>
#include <boost/iostreams/filter/gzip.hpp>
#include "Logger.hpp"
#include "Simulator.hpp"
#include "SimulationCase.hpp"
#include "SimTask.hpp"
#include "MemoryManager.hpp"
#include "JobStatistics.hpp"
#include "TrafficStatistics.hpp"
#include "AvailabilityStatistics.hpp"
#include "StretchStatistics.hpp"
#include "PerfectScheduler.hpp"
#include "FailureGenerator.hpp"
#include "portable_binary_oarchive.hpp"
using namespace log4cpp;
using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::gregorian;


int Simulator::Event::lastEventId = 0;


void LogMsg::log(const char * category, int priority, LogMsg::AbstractTypeContainer * values) {
    if (category == "Sim.Progress")
        Simulator::getInstance().progressLog(values);
    else
        Simulator::getInstance().log(category, priority, values);
}

// TODO: log with XBT
void Simulator::log(const char * category, int priority, LogMsg::AbstractTypeContainer* values) {
    if (debugFile.is_open() && Category::getInstance(category).isPriorityEnabled(priority)) {
        Duration realTime(getRealTime().total_microseconds());
        Time curTime = time;
        debugArchive << realTime << ' ' << curTime << ' ';
        if(currentNode != NULL)
            debugArchive << currentNode->getLocalAddress() << ',';
        else
            debugArchive << "sim.control ";
        debugArchive << category << '(' << priority << ')' << ' ';
        for (LogMsg::AbstractTypeContainer * it = values; it != NULL; it = it->next)
            debugArchive << *it;
        debugArchive << endl;
    }
}

// TODO: log with XBT
void Simulator::progressLog(LogMsg::AbstractTypeContainer * values) {
    cout << '#' << getpid() << ": ";
    for (LogMsg::AbstractTypeContainer * it = values; it != NULL; it = it->next)
        cout << *it;
    cout << endl;
    
    if (progressFile.is_open()) {
        for (LogMsg::AbstractTypeContainer * it = values; it != NULL; it = it->next)
            progressFile << *it;
        progressFile << endl;
    }
}


void finish(int param) {
    cout << "Stopping due to user signal" << endl;
    Simulator::getInstance().stop();
}


int main(int argc, char * argv[]) {
#ifdef __x86_64__
    cout << "PeerCompSimpleSim 64bits #" << getpid() << endl;
#else
    cout << "PeerCompSimpleSim 32bits #" << getpid() << endl;
#endif
    if (argc != 2) {
        cout << "Usage: PeerCompSimpleSim config_file" << endl;
        return 1;
    }

    // Avoid huge core dumps on error
#ifndef WITH_CORE_DUMP
    rlimit zeroLimit;
    zeroLimit.rlim_cur = zeroLimit.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &zeroLimit);
#endif

    ptime start = microsec_clock::local_time();
    MemoryManager::getInstance().reset();
    Simulator & sim = Simulator::getInstance();

    Properties property;
    property.loadFromFile(string(argv[1]));
    shared_ptr<SimulationCase> simCase = CaseFactory::getInstance().createCase(property("case_name", string("")), property);
    if (simCase.get()) {
        std::signal(SIGUSR1, finish);
        sim.registerHandler(simCase);
        sim.setProperties(property);
        if (sim.isPrepared()) {
            sim.getPStats().startEvent("Prepare simulation case");
            simCase->preStart();
            sim.getPStats().endEvent("Prepare simulation case");
            LogMsg("Sim.Progress", 0) << MemoryManager::getInstance().getMaxUsedMemory() << " bytes to prepare simulation case.";
            sim.run();
            sim.showStatistics();
            PeerCompNode::showTree(log4cpp::Priority::INFO);
            PeerCompNode::checkTree();
            simCase->postEnd();
            ptime end = microsec_clock::local_time();
            size_t mem = MemoryManager::getInstance().getMaxUsedMemory();
            LogMsg("Sim.Progress", 0) << "Ending test at " << end << ". Lasted " << (end - start) << " and used " << mem << " bytes.";
        }
    } else {
        LogMsg("Sim.Progress", 0) << "ERROR: No test exists with name \"" << property("case_name", string("")) << '"';
    }

    return 0;
}


Simulator::Simulator() {
    // Statistics:
    numEvents = 0;
    totalBytesSent = 0;
    numMsgSent = 0;

    // Simulation variables
    p = NULL;
    currentNode = NULL;
    time = Time();
    real_time = seconds(0);
    doStop = false;
    inactiveEvents = 0;
}


Simulator::~Simulator() {
    // Delete remaining events
    while (!events.empty()) {
        p = events.top();
        events.pop();
        delete p;
    }
    interEventHandlers.clear();
    // Clean node state
    for (vector<PeerCompNode>::iterator it = routingTable.begin(); it != routingTable.end(); it++)
        it->finish();
}


static bool checkLogFile(const fs::path & logFile) {
    fs::ifstream ifs(logFile);
    if (!ifs.good()) {
        return false;
    }
    ifs.seekg(0, ios::end);
    ifs.unget();
    while (ifs.good() && ifs.peek() != '\n') ifs.unget();
    ifs.unget();
    while (ifs.good() && ifs.peek() != '\n') ifs.unget();
    ifs.ignore(1);
    string line;
    getline(ifs, line);
    return line.find("Ending test at") != string::npos;
}


void Simulator::setProperties(Properties & property) {
    static unsigned int defaultSeed = 12345;

    // Set logging
    resultDir = property("results_dir", std::string("./results"));
    if (!fs::exists(resultDir)) fs::create_directories(resultDir);
    fs::path logFile("execution.log");
    if (fs::exists(resultDir / logFile) && !property("overwrite", false) && checkLogFile(resultDir / logFile)) {
        LogMsg("Sim.Progress", 0) << "Log file exists at " << (resultDir / logFile);
        doStop = true;
        return;
    }
    LogMsg("Sim.Progress", 0) << "Logging to " << (resultDir / logFile);

    progressFile.open(resultDir / logFile);
    debugFile.open(resultDir / "debug.log.gz");
    if (debugFile.is_open()) {
        debugArchive.push(boost::iostreams::gzip_compressor());
        debugArchive.push(debugFile);
    }
    LogMsg::initLog(property("log_conf_string", string("")));
    LogMsg("Sim.Progress", 0) << "Running simulation test at " << microsec_clock::local_time() << ": " << property;

    pstats.openFile(resultDir);
    pstats.startEvent("Prepare simulation network");

    // Simulation variables
    measureSize = property("measure_size", true);
    maxEvents = property("max_events", 0ULL);
    maxRealTime = seconds(property("max_time", 0));
    maxSimTime = Duration(property("max_sim_time", 0.0));
    maxMemUsage = property("max_mem", 0U);
    srand(property("seed", defaultSeed));
    showStep = property("show_step", 100000);
    minDelay = property("min_delay", 0.05);  // 50ms min delay
    maxDelay = property("max_delay", 0.3);   // 300ms max delay
    unsigned int numNodes = property("num_nodes", (unsigned int)0);
    iface.resize(numNodes);
    double minInBW = property("min_down_bw", 125000.0),
                     maxInBW = property("max_down_bw", 125000.0),
                               stepInBW = property("step_down_bw", 1),
                                          minOutBW = property("min_up_bw", 125000.0),
                                                     maxOutBW = property("max_up_bw", 125000.0),
                                                                stepOutBW = property("step_up_bw", 1);
    for (unsigned int i = 0; i < numNodes; i++) {
        iface[i].inBW = Simulator::uniform(minInBW, maxInBW, stepInBW);
        iface[i].outBW = Simulator::uniform(minOutBW, maxOutBW, stepOutBW);
    }

    // Library config
    ConfigurationManager::getInstance().setUpdateBandwidth(property("update_bw", 1000.0));
    ConfigurationManager::getInstance().setStretchRatio(property("stretch_ratio", 2.0));
    ConfigurationManager::getInstance().setHeartbeat(property("heartbeat", 300));
    ConfigurationManager::getInstance().setWorkingPath(resultDir);

    // Node creation
    PeerCompNodeFactory factory(property);
    routingTable.resize(numNodes);
    for (unsigned int i = 0; i < numNodes; i++) {
        currentNode = &routingTable[i];
        factory.setupNode(i, *currentNode);
    }

    // Perfect scheduler
    ps = PerfectScheduler::createScheduler(property("perfect_scheduler", string("")));
    if (ps.get())
        interEventHandlers.push_back(ps);

    // Failure generator
    if (property.count("mtbf")) {
        interEventHandlers.push_back(shared_ptr<InterEventHandler>(new FailureGenerator(property("mtbf", 1000.0), property("min_failed_nodes", 1),
                                     property("max_failed_nodes", 1), property("max_failures", -1))));
    }

    // Statistics
    interEventHandlers.push_back(shared_ptr<InterEventHandler>(new JobStatistics()));
    interEventHandlers.push_back(shared_ptr<InterEventHandler>(new TrafficStatistics()));
    interEventHandlers.push_back(shared_ptr<InterEventHandler>(new AvailabilityStatistics()));
    if (routingTable[0].getSchedulerType() == PeerCompNode::MinStretchSchedulerClass)
        interEventHandlers.push_back(shared_ptr<InterEventHandler>(new StretchStatistics()));

    pstats.endEvent("Prepare simulation network");
    LogMsg("Sim.Progress", 0) << MemoryManager::getInstance().getMaxUsedMemory() << " bytes to prepare simulation network.";
}


void Simulator::stepForward() {
    // Do not take into account the canceled and delayed timers
    while (!events.empty()) {
        p = events.top();
        events.pop();
        if (!p->active) {
            inactiveEvents--;
            delete p;
        } else {
            time = p->t;
            // Measure operation duration if the event is blocked
            opStart = microsec_clock::local_time();
            currentNode = &routingTable[p->to];
            generatedEvents.clear();

            // See if the message is captured
            bool blocked = false;
            for (list<shared_ptr<InterEventHandler> >::iterator it = interEventHandlers.begin(); !blocked && it != interEventHandlers.end(); it++)
                blocked |= (*it)->blockEvent(*p);
            if (blocked) {
                // Erase from timers list, if it is there
                if (p->from == p->to && p->size == 0) timers.erase(p->id);
                delete p;
                continue;
            }

            // Not captured
            if (p->size && p->from != p->to && !p->inRecvQueue) {
                // Not a self-message, check incoming queue
                totalBytesSent += p->size;
                NodeNetInterface & dstIface = iface[p->to];
                dstIface.inQueueFreeTime += p->txDuration;
                if (dstIface.inQueueFreeTime <= p->t) {
                    dstIface.inQueueFreeTime = p->t;
                } else {
                    // Delay the message in the incoming queue
                    p->t = dstIface.inQueueFreeTime;
                    p->inRecvQueue = true;
                    events.push(p);
                    continue;
                }
            }

            // Deal with the event
            numEvents++;
            LogMsg("Sim.Event", INFO) << "";
            LogMsg("Sim.Event", INFO) << "###################################";
            LogMsg("Sim.Event", INFO) << "Event #" << numEvents
            << ": " << *p->msg
            << " at " << time
            << " from " << AddrIO(p->from)
            << " to " << AddrIO(p->to);
            for (list<shared_ptr<InterEventHandler> >::iterator it = interEventHandlers.begin(); it != interEventHandlers.end(); it++)
                (*it)->beforeEvent(*p);
            pstats.startEvent(p->msg->getName());
            // Measure operation duration from this point
            opStart = microsec_clock::local_time();
            routingTable[p->to].receiveMessage(p->from, p->msg);
            pstats.endEvent(p->msg->getName());
            for (list<shared_ptr<InterEventHandler> >::iterator it = interEventHandlers.begin(); it != interEventHandlers.end(); it++)
                (*it)->afterEvent(*p);
            // Erase from timers list, if it is there
            if (p->from == p->to && p->size == 0) timers.erase(p->id);
            delete p;
            break;
        }
    }
    currentNode = NULL;
    p = NULL;
}


void Simulator::run() {
    SimulationCase & simCase = *static_pointer_cast<SimulationCase>(interEventHandlers.front());
    start = microsec_clock::local_time();
    ptime realStart = start;
    while (!events.empty() && !doStop && simCase.doContinue()) {
        if (maxEvents && numEvents >= maxEvents) {
            LogMsg("Sim.Progress", 0) << "Maximum number of events limit reached: " << maxEvents;
            break;
        } else if (maxRealTime > seconds(0) && microsec_clock::local_time() - realStart >= maxRealTime) {
            LogMsg("Sim.Progress", 0) << "Maximum real time limit reached: " << maxRealTime;
            break;
        } else if (maxSimTime > Duration(0.0) && time - Time() >= maxSimTime) {
            LogMsg("Sim.Progress", 0) << "Maximum simulation time limit reached: " << maxSimTime;
            break;
        } else if (maxMemUsage && numEvents % 1000 == 0 && (MemoryManager::getInstance().getMaxUsedMemory() >> 20) > maxMemUsage) {
            LogMsg("Sim.Progress", 0) << "Maximum memory usage limit reached: " << maxMemUsage;
            break;
        }
        stepForward();
        if (numEvents % showStep == 0) {
            end = microsec_clock::local_time();
            real_time += end - start;
            double real_duration = (end - start).total_microseconds() / 1000000.0;
            start = end;
            // Show statistics
            LogMsg("Sim.Progress", 0) << real_time << " (" << time << ")   "
            << numEvents << " ev (" << (showStep / real_duration) << " ev/s)   "
            << MemoryManager::getInstance().getUsedMemory() << " mem   "
            << simCase.getCompletedPercent() << "%   "
            << SimTask::getRunningTasks() << " tasks";
            pstats.savePartialStatistics();
        }
    }
    end = microsec_clock::local_time();
    real_time += end - start;
}


unsigned long int Simulator::getMsgSize(shared_ptr<BasicMsg> msg) {
    unsigned long int size = 0;
    try {
        std::stringstream ss;
        portable_binary_oarchive oa(ss);
        BasicMsg * tmp = msg.get();
        oa << tmp;
        size = ss.tellp();
    } catch (...) {
        LogMsg("Sim.Progress", WARN) << "Error serializing message of type " << msg->getName();
    }
    return size;
}


unsigned int Simulator::sendMessage(uint32_t src, uint32_t dst, shared_ptr<BasicMsg> msg) {
    // NOTE: msg must not be clonned, to track messages.
    bool blocked = false;
    for (list<shared_ptr<InterEventHandler> >::iterator it = interEventHandlers.begin(); !blocked && it != interEventHandlers.end(); it++)
        blocked |= (*it)->blockMessage(src, dst, msg);
    if (blocked) return 0;

    numMsgSent++;
    // Delay follows pareto distribution of k=2
    static const double kDelay = 2.0;     // k = 2 for Pareto distribution

    // Measure operation duration, only if inEvent().
    Duration opDuration(inEvent() ? (microsec_clock::local_time() - opStart).total_microseconds() : 0.0);

    Event * event;
    unsigned long int size = 0;
    if (src != dst) {
        if (measureSize) {
            size = getMsgSize(msg) + 90;   // 90 bytes of Ethernet + IP + TCP
        }
        NodeNetInterface & srcIface = iface[src];
        NodeNetInterface & dstIface = iface[dst];
        if (srcIface.outQueueFreeTime <= time) {
            srcIface.outQueueFreeTime = time;
        }
        Duration txTime(size / (srcIface.outBW < dstIface.inBW ? srcIface.outBW : dstIface.inBW));
        event = new Event(time + opDuration, srcIface.outQueueFreeTime, txTime, Duration(pareto(minDelay, kDelay, maxDelay)), msg, size);
        srcIface.outQueueFreeTime += txTime;
    } else {
        event = new Event(time + opDuration, Duration(), msg, 0);
    }
    event->to = dst;
    event->from = src;
    events.push(event);
    generatedEvents.push_back(event);
    return size;
}


unsigned int Simulator::injectMessage(uint32_t src, uint32_t dst, shared_ptr<BasicMsg> msg,
                                      Duration d, bool withOpDuration) {
    // NOTE: msg must not be clonned, to track messages.
    numMsgSent++;

    unsigned long int size = 0;
    if (measureSize) {
        size = getMsgSize(msg);
    }
    if (withOpDuration)
        d += Duration((microsec_clock::local_time() - opStart).total_microseconds());
    Event * event = new Event(time + d, Duration(), msg, size);
    event->to = dst;
    event->from = src;
    events.push(event);
    return size;
}


int Simulator::setTimer(uint32_t dst, Time when, shared_ptr<BasicMsg> msg) {
    bool blocked = false;
    for (list<shared_ptr<InterEventHandler> >::iterator it = interEventHandlers.begin(); !blocked && it != interEventHandlers.end(); it++)
        blocked |= (*it)->blockMessage(dst, dst, msg);
    if (blocked) return 0;

    Event * event = new Event(when, msg, 0);
    event->to = dst;
    event->from = dst;
    timers[event->id] = event;
    events.push(event);
    generatedEvents.push_back(event);
    return event->id;
}


void Simulator::cancelTimer(int timerId) {
    map<int, Event *>::iterator timer = timers.find(timerId);
    if (timer != timers.end()) {
        timer->second->active = false;
        timers.erase(timer);
        inactiveEvents++;
    }
}


void Simulator::showStatistics() {
    // Show statistics
    double real_duration = real_time.total_microseconds() / 1000000.0;
    LogMsg("Sim.Progress", 0) << real_time
    << " (" << time << ", " << (time.getRawDate() / 1000000.0 / real_duration) << " sims/s)   "
    << numEvents << " ev (" << (numEvents / real_duration) << " ev/s)   "
    << totalBytesSent << " trf (" << numMsgSent << " msg, " << ((double)totalBytesSent / numMsgSent) << " B/msg, "
    << ((totalBytesSent / (time.getRawDate() / 1000000.0)) / routingTable.size()) << " Bps/node)   "
    << MemoryManager::getInstance().getUsedMemory() << " mem   100%";
    pcstats.saveTotalStatistics();
    pstats.saveTotalStatistics();
}
