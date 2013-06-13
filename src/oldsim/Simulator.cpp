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

#include <sys/resource.h>
#include <unistd.h>
#include <csignal>
#include <iostream>
#include <algorithm>
#include <boost/iostreams/filter/gzip.hpp>
#include <log4cpp/Category.hh>
#include <google/heap-profiler.h>
#include "config.h"
#include "Logger.hpp"
#include "Simulator.hpp"
#include "SimulationCase.hpp"
#include "SimTask.hpp"
#include "MemoryManager.hpp"
#include "TrafficStatistics.hpp"
#include "AvailabilityStatistics.hpp"
#include "CentralizedScheduler.hpp"
#include "FailureGenerator.hpp"
#include "SignalException.hpp"
using namespace log4cpp;
using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;


int Simulator::Event::lastEventId = 0;


bool Simulator::isLogEnabled(const std::string & category, int priority) {
    return debugFile.is_open() && Category::getInstance(category).isPriorityEnabled(priority) && (!debugNode || currentNode == debugNode);
}


void LogMsg::log(const char * category, int priority, AbstractTypeContainer * values) {
    Simulator & sim = Simulator::getInstance();
    if (category == std::string("Sim.Progress")) {
        ostringstream oss;
        for (AbstractTypeContainer * it = values; it != NULL; it = it->next)
            oss << *it;
        sim.progressLog(oss.str());
    }
    else {
        if (sim.isLogEnabled(category, priority)) {
            boost::iostreams::filtering_ostream & debugArchive = sim.getDebugArchive();
            Duration realTime(sim.getRealTime().total_microseconds());
            Time curTime = sim.getCurrentTime();
            debugArchive << realTime << ' ' << curTime << ' ';
            if(sim.inEvent())
                debugArchive << sim.getCurrentNode().getLocalAddress() << ' ';
            else
                debugArchive << "sim.control ";
            debugArchive << category << '(' << priority << ')' << ' ';

            for (AbstractTypeContainer * it = values; it != NULL; it = it->next)
                debugArchive << *it;

            debugArchive << endl;
        }
    }
}


void Simulator::progressLog(const string & msg) {
    cout << '#' << getpid() << ": " << msg << endl;

    if (progressFile.is_open()) {
        progressFile << msg << endl;
    }
}


void finish(int param) {
    cout << "Stopping due to user signal" << endl;
    Simulator::getInstance().stop();
}


void showInformation(int param) {
    Simulator::getInstance().showInformation();
    std::signal(SIGUSR1, showInformation);
}


int main(int argc, char * argv[]) {
#ifdef __x86_64__
    int bits = 64;
#else
    int bits = 32;
#endif
    cout << "STaRS simple simulator, build " << STARS_BUILD_TYPE << " " << bits << "bits #" << getpid() << endl;
#ifndef NDEBUG
    cout << "NOTE: Debug versions of STaRS simulator do not account for computation time." << endl;
#endif
    if (argc != 2) {
        cout << "Usage: stars-oldsim config_file" << endl;
        return 1;
    }

    // Avoid huge core dumps on error
#ifndef WITH_CORE_DUMP
    rlimit zeroLimit;
    zeroLimit.rlim_cur = zeroLimit.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &zeroLimit);
#endif

    try {
        SignalException::Handler::getInstance().setHandler();
        ptime start = microsec_clock::local_time();
        MemoryManager::getInstance().reset();
        Simulator & sim = Simulator::getInstance();

        Properties property;
        string src(argv[1]);
        if (src == "-")
            property.loadFrom(cin);
        else {
            fs::ifstream ifs(src);
            property.loadFrom(ifs);
        }
        sim.setProperties(property);
        if (sim.isPrepared()) {
            std::signal(SIGUSR1, showInformation);
            std::signal(SIGINT, finish);
            std::signal(SIGTERM, finish);
            sim.getPerfStats().startEvent("Prepare simulation case");
            sim.getSimulationCase()->preStart();
            sim.getPerfStats().endEvent("Prepare simulation case");
            LogMsg("Sim.Progress", 0) << MemoryManager::getInstance().getMaxUsedMemory() << " bytes to prepare simulation case.";
            if (property("profile_heap", false))
                HeapProfilerStart((sim.getResultDir() / "hprof").native().c_str());
            sim.run();
            sim.showStatistics();
            sim.getSimulationCase()->postEnd();
            sim.finish();
            ptime end = microsec_clock::local_time();
            size_t mem = MemoryManager::getInstance().getMaxUsedMemory();
            LogMsg("Sim.Progress", 0) << "Ending test at " << end << ". Lasted " << (end - start) << " and used " << mem << " bytes.";
            if (property("profile_heap", false))
                HeapProfilerStop();
        }
    } catch (SignalException & e) {
        SignalException::Handler::getInstance().printStackTrace();
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
    start = microsec_clock::local_time();
    doStop = false;
}


void Simulator::finish() {
    // Delete remaining events
    while (!events.empty()) {
        p = events.top();
        events.pop();
        delete p;
    }
    // Clean node state
    for (vector<StarsNode>::iterator it = routingTable.begin(); it != routingTable.end(); it++)
        it->finish();
}


void Simulator::showInformation() {
    LogMsg("Sim.Progress", 0) << getSimulationCase()->getProperties();
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

    simCase = CaseFactory::getInstance().createCase(property("case_name", string("")), property);
    if (!simCase.get()) {
        // If no simulation case exists with that name, return
        LogMsg("Sim.Progress", 0) << "ERROR: No test exists with name \"" << property("case_name", string("")) << '"';
        doStop = true;
        return;
    }

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
    // Network delay in (50ms, 300ms) by default
    // Delay follows pareto distribution of k=2
    static const double kDelay = 2.0;
    netDelay = ParetoVariable(property("min_delay", 0.05), kDelay, property("max_delay", 0.3));
    unsigned int numNodes = property("num_nodes", (unsigned int)0);
    iface.resize(numNodes);
    DiscreteUniformVariable inBWVar(property("min_down_bw", 125000.0), property("max_down_bw", 125000.0), property("step_down_bw", 1)),
        outBWVar(property("min_up_bw", 125000.0), property("max_up_bw", 125000.0), property("step_up_bw", 1));
    for (unsigned int i = 0; i < numNodes; i++) {
        iface[i].inBW = inBWVar();
        iface[i].outBW = outBWVar();
    }

    // Library config
    StarsNode::libStarsConfigure(property);

    // Statistics
    sstats.openStatsFiles(resultDir);
    sstats.setNumNodes(numNodes);
    tstats.setNumNodes(numNodes);

    // Node creation
    routingTable.resize(numNodes);
    for (unsigned int i = 0; i < numNodes; i++) {
        currentNode = &routingTable[i];
        currentNode->setup(i);
    }

    debugNode = property.count("debug_node") ? &routingTable[0] + property("debug_node", 0) : NULL;

    // Centralized scheduler
    cs = CentralizedScheduler::createScheduler(property("cent_scheduler", string("")));

    // Failure generator
    if (property.count("median_session")) {
        fg.startFailures(property("median_session", 1.0),
                property("min_failed_nodes", 1),
                property("max_failed_nodes", 1));
    } else if (property.count("big_fail_at")) {
        fg.bigFailure(Duration(property("big_fail_at", 1.0)),
                property("min_failed_nodes", 1),
                property("max_failed_nodes", 1));
    }

//	interEventHandlers.push_back(shared_ptr<InterEventHandler>(new AvailabilityStatistics()));

    pstats.endEvent("Prepare simulation network");
    LogMsg("Sim.Progress", 0) << MemoryManager::getInstance().getMaxUsedMemory() << " bytes to prepare simulation network.";
}


void Simulator::stepForward() {
    // Do not take into account the canceled and delayed timers
    while (!events.empty()) {
        p = events.top();
        events.pop();
        time = p->t;
        // Measure operation duration if the event is blocked
        opStart = microsec_clock::local_time();
        currentNode = &routingTable[p->to];
        generatedEvents.clear();

        // See if the message is captured
        if ((cs.get() && cs->blockEvent(*p)) || fg.isNextFailure(*p->msg)) {
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
        tstats.msgReceived(p->from, p->to, p->size, iface[p->to].inBW, *p->msg);
        simCase->beforeEvent(p->from, p->to, *p->msg);
        pstats.startEvent(p->msg->getName());
        // Measure operation duration from this point
        opStart = microsec_clock::local_time();
        routingTable[p->to].receiveMessage(p->from, p->msg);
        pstats.endEvent(p->msg->getName());
        simCase->afterEvent(p->from, p->to, *p->msg);
        delete p;
        break;
    }
    currentNode = NULL;
    p = NULL;
}


void Simulator::run() {
    start = microsec_clock::local_time();
    ptime realStart = start;
    time_facet * f = new time_facet();
    f->time_duration_format("%O:%M:%S");
    stringstream realTimeText;
    realTimeText.imbue(locale(locale::classic(), f));
    while (!events.empty() && !doStop && simCase->doContinue()) {
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
            realTimeText.str("");
            realTimeText << real_time;
            double real_duration = (end - start).total_microseconds() / 1000000.0;
            start = end;
            // Show statistics
            LogMsg("Sim.Progress", 0) << realTimeText.str() << " (" << time << ")   "
                << numEvents << " ev (" << (showStep / real_duration) << " ev/s)   "
                << MemoryManager::getInstance().getUsedMemory() << " mem   "
                << simCase->getCompletedPercent() << "%   "
                << sstats.getExistingTasks() << " tasks";
            pstats.savePartialStatistics();
        }
    }
    end = microsec_clock::local_time();
    real_time += end - start;
}


unsigned long int Simulator::getMsgSize(boost::shared_ptr<BasicMsg> msg) {
    ostringstream oss;
    msgpack::packer<std::ostream> pk(&oss);
    msg->pack(pk);
    return oss.tellp();
}


unsigned int Simulator::sendMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg) {
    // NOTE: msg must not be clonned, to track messages.
    if (cs.get() && cs->blockMessage(msg)) return 0;

    numMsgSent++;

    Duration opDuration;
#ifdef NDEBUG
    // Measure operation duration, only if inEvent().
    opDuration = Duration((int64_t)(inEvent() ? (microsec_clock::local_time() - opStart).total_microseconds() : 0));
#endif

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
        event = new Event(time + opDuration, srcIface.outQueueFreeTime, txTime, Duration(netDelay()), msg, size);
        srcIface.outQueueFreeTime += txTime;
        tstats.msgSent(src, dst, size, srcIface.outBW, srcIface.outQueueFreeTime + txTime, *msg);
    } else {
        event = new Event(time + opDuration, Duration(), msg, 0);
    }
    event->to = dst;
    event->from = src;
    events.push(event);
    generatedEvents.push_back(event);
    return size;
}


unsigned int Simulator::injectMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg,
        Duration d, bool withOpDuration) {
    // NOTE: msg must not be clonned, to track messages.
    numMsgSent++;

    unsigned long int size = 0;
    if (src != dst && measureSize) {
        size = getMsgSize(msg);
    }
#ifdef NDEBUG
    if (withOpDuration)
        d += Duration((microsec_clock::local_time() - opStart).total_microseconds());
#endif
    Event * event = new Event(time + d, Duration(), msg, size);
    event->to = dst;
    event->from = src;
    events.push(event);
    return size;
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
    sstats.saveTotalStatistics();
    pstats.saveTotalStatistics();
    tstats.saveTotalStatistics();
}
