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
#include <boost/iostreams/tee.hpp>
#include <log4cpp/Category.hh>
#include <google/heap-profiler.h>
#include "config.h"
#include "Logger.hpp"
#include "Simulator.hpp"
#include "SimulationCase.hpp"
#include "SimTask.hpp"
#include "util/MemoryManager.hpp"
#include "TrafficStatistics.hpp"
#include "AvailabilityStatistics.hpp"
#include "CentralizedScheduler.hpp"
#include "FailureGenerator.hpp"
#include "util/SignalException.hpp"
using namespace log4cpp;
using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;


int Simulator::Event::lastEventId = 0;


bool Simulator::isLogEnabled(const std::string & category, int priority) {
    return debugFile.is_open() && Category::getInstance(category).isPriorityEnabled(priority) && (!debugNode || currentNode == debugNode);
}


std::ostream * Logger::streamIfEnabled(const char * category, int priority) {
    Simulator & sim = Simulator::getInstance();
    if (category == std::string("Sim.Progress")) {
        boost::iostreams::filtering_ostream & pstream = sim.getProgressStream();
        pstream << '#' << getpid() << ": ";
        return &pstream;
    } else if (sim.isLogEnabled(category, priority)) {
        boost::iostreams::filtering_ostream & debugArchive = sim.getDebugStream();
        Duration realTime(sim.getRealTime().total_microseconds());
        Time curTime = sim.getCurrentTime();
        if (!sim.isLastLogMoment()) {
            debugArchive << endl << realTime << ' ' << curTime << ' ';
            if(sim.inEvent())
                debugArchive << sim.getCurrentNode().getLocalAddress() << ' ';
            else
                debugArchive << "sim.control ";
            debugArchive << endl;
        }
        ostringstream oss;
        oss << "    " << category << '(' << priority << ')' << ' ';
        debugArchive << oss.str();
        Logger::setIndent(oss.tellp());
        return &debugArchive;
    } else return nullptr;
}


bool Simulator::isLastLogMoment() {
    if (inEvent()) {
        if (lastDebugTime == time && lastDebugNode == currentNode) return true;
        else {
            lastDebugTime = time;
            lastDebugNode = currentNode;
        }
    } else {
        if (lastDebugTime == time && lastDebugNode == NULL) return true;
        else {
            lastDebugTime = time;
            lastDebugNode = NULL;
        }
    }
    return false;
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
            Logger::msg("Sim.Progress", 0, MemoryManager::getInstance().getMaxUsedMemory(), " bytes to prepare simulation case.");
            if (property("profile_heap", false))
                HeapProfilerStart((sim.getResultDir() / "hprof").native().c_str());
            sim.run();
            sim.showStatistics();
            sim.getSimulationCase()->postEnd();
            sim.finish();
            ptime end = microsec_clock::local_time();
            size_t mem = MemoryManager::getInstance().getMaxUsedMemory();
            Logger::msg("Sim.Progress", 0, "Ending test at ", end, ". Lasted ", (end - start), " and used ", mem, " bytes.");
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
    lastDebugNode = NULL;
    time = Time();
    lastDebugTime = Time(-1);
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
    Logger::msg("Sim.Progress", 0, getSimulationCase()->getProperties());
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

    progressStream.push(cout);
    simCase = CaseFactory::getInstance().createCase(property("case_name", string("")), property);
    if (!simCase.get()) {
        // If no simulation case exists with that name, return
        Logger::msg("Sim.Progress", 0, "ERROR: No test exists with name \"", property("case_name", string("")), '"');
        doStop = true;
        return;
    }

    // Set logging
    resultDir = property("results_dir", std::string("./results"));
    if (!fs::exists(resultDir)) fs::create_directories(resultDir);
    fs::path logFile("execution.log");
    if (fs::exists(resultDir / logFile) && !property("overwrite", false) && checkLogFile(resultDir / logFile)) {
        Logger::msg("Sim.Progress", 0, "Log file exists at ", resultDir / logFile);
        doStop = true;
        return;
    }
    Logger::msg("Sim.Progress", 0, "Logging to ", resultDir / logFile);

    progressFile.open(resultDir / logFile);
    if (progressFile.is_open()) {
        progressStream.reset();
        progressStream.push(boost::iostreams::tee_filter<fs::ofstream>(progressFile));
        progressStream.push(cout);
    }
    debugFile.open(resultDir / "debug.log.gz");
    if (debugFile.is_open()) {
        debugStream.push(boost::iostreams::gzip_compressor());
        debugStream.push(debugFile);
    }
    Logger::initLog(property("log_conf_string", string("")));
    Logger::msg("Sim.Progress", 0, "Running simulation test at ", microsec_clock::local_time(), ": ", property);

    pstats.openFile(resultDir);
    pstats.startEvent("Prepare simulation network");

    // Simulation variables
    measureSize = property("measure_size", true);
    maxRealTime = seconds(property("max_time", 0));
    maxSimTime = Duration(property("max_sim_time", 0.0));
    maxMemUsage = property("max_mem", 0U);
    srand(property("seed", defaultSeed));
    showStep = property("show_step", 10000);
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
    tstats.setNumNodes(numNodes);

    // Node creation
    routingTable.resize(numNodes);
    for (unsigned int i = 0; i < numNodes; i++) {
        currentNode = &routingTable[i];
        currentNode->setup(i);
    }

    debugNode = property.count("debug_node") ? &routingTable[0] + property("debug_node", 0) : NULL;

    // Centralized scheduler
    cs = StarsNode::Configuration::getInstance().getPolicy()->getCentScheduler();

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
    Logger::msg("Sim.Progress", 0, MemoryManager::getInstance().getMaxUsedMemory(), " bytes to prepare simulation network.");
}


bool Simulator::captured() {
    // Measure operation duration if the event is captured
    opStart = microsec_clock::local_time();
    if ((cs.get() && cs->blockEvent(*p)) || fg.isNextFailure(*p->msg)) {
        delete p;
        return true;
    } else return false;
}


bool Simulator::enqueued() {
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
            return true;
        }
    }
    return false;
}


void Simulator::popNextEvent() {
    p = events.top();
    events.pop();
    time = p->t;
    currentNode = &routingTable[p->to];
    generatedEvents.clear();
}


void Simulator::stepForward() {
    popNextEvent();
    while (captured() || enqueued()) {
        if (events.empty())
            return;
        popNextEvent();
    }
    numEvents++;
    Logger::msg("Sim.Event", INFO, "");
    Logger::msg("Sim.Event", INFO, "###################################");
    Logger::msg("Sim.Event", INFO, "Event #", numEvents, ": ", *p->msg,
            " at ", time, " from ", AddrIO(p->from), " to ", AddrIO(p->to));
    pstats.endEvent("Event selection");

    pstats.startEvent("Before event");
    tstats.msgReceived(p->from, p->to, p->size, iface[p->to].inBW, *p->msg);
    simCase->beforeEvent(p->from, p->to, *p->msg);
    pstats.endEvent("Before event");

    pstats.startEvent(p->msg->getName());
    // Measure operation duration from this point
    opStart = microsec_clock::local_time();
    routingTable[p->to].receiveMessage(p->from, p->msg);
    pstats.endEvent(p->msg->getName());

    pstats.startEvent("After event");
    simCase->afterEvent(p->from, p->to, *p->msg);
    pstats.endEvent("After event");

    currentNode = NULL;
    delete p;
    p = NULL;
}


void Simulator::run() {
    start = microsec_clock::local_time();
    ptime realStart = start, nextShow = start;
    unsigned long int lastNumEvents = 0;
    time_facet * f = new time_facet();
    f->time_duration_format("%O:%M:%S");
    stringstream realTimeText;
    realTimeText.imbue(locale(locale::classic(), f));
    while (!events.empty() && !doStop && simCase->doContinue()) {
        ptime currentTime = microsec_clock::local_time();
        if (maxRealTime > seconds(0) && currentTime - realStart >= maxRealTime) {
            Logger::msg("Sim.Progress", 0, "Maximum real time limit reached: ", maxRealTime);
            break;
        } else if (maxSimTime > Duration(0.0) && time - Time() >= maxSimTime) {
            Logger::msg("Sim.Progress", 0, "Maximum simulation time limit reached: ", maxSimTime);
            break;
        } else if (maxMemUsage && numEvents % 1000 == 0 && (MemoryManager::getInstance().getMaxUsedMemory() >> 20) > maxMemUsage) {
            Logger::msg("Sim.Progress", 0, "Maximum memory usage limit reached: ", maxMemUsage);
            break;
        }
        stepForward();
        if (currentTime >= nextShow) {
            while (currentTime >= nextShow) nextShow += milliseconds(showStep);
            end = currentTime;
            real_time += end - start;
            realTimeText.str("");
            realTimeText << real_time;
            double speed = (numEvents - lastNumEvents) / ((end - start).total_microseconds() / 1000000.0);
            lastNumEvents = numEvents;
            start = end;
            // Show statistics
            Logger::msg("Sim.Progress", 0, realTimeText.str(), " (", time, ")   ",
                numEvents, " ev (", speed, " ev/s)   ",
                MemoryManager::getInstance().getUsedMemory(), " mem   ",
                simCase->getCompletedPercent(), "%   ",
                sstats.getExistingTasks(), " tasks, ",
                sstats.getRunningTasks(), " running");
            pstats.savePartialStatistics();
        }
    }
    end = microsec_clock::local_time();
    real_time += end - start;
}


unsigned long int Simulator::getMsgSize(std::shared_ptr<BasicMsg> msg) {
    static struct CountBuffer : public std::streambuf {
        streamsize count;
        CountBuffer() : streambuf(), count(0) {}
        streamsize xsputn (const char* s, streamsize n) {
            count += n;
            return n;
        }
    } buf;
    static ostream os(&buf);
    buf.count = 0;
    msgpack::packer<std::ostream> pk(&os);
    msg->pack(pk);
    return buf.count;
}


unsigned int Simulator::sendMessage(uint32_t src, uint32_t dst, std::shared_ptr<BasicMsg> msg) {
    // NOTE: msg must not be clonned, to track messages.
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
            pstats.startEvent("getMsgSize");
            size = getMsgSize(msg) + 90;   // 90 bytes of Ethernet + IP + TCP
            pstats.endEvent("getMsgSize");
        }
        NodeNetInterface & srcIface = iface[src];
        NodeNetInterface & dstIface = iface[dst];
        if (srcIface.outQueueFreeTime <= time) {
            srcIface.outQueueFreeTime = time;
        }
        Duration txTime(size / (srcIface.outBW < dstIface.inBW ? srcIface.outBW : dstIface.inBW));
        Duration delay(netDelay());
//        if (msg->getName() == "FSPAvailabilityInformation") {
//            delay = Duration(0.0);
//        }
        event = new Event(time + opDuration, srcIface.outQueueFreeTime, txTime, delay, msg, size);
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


unsigned int Simulator::injectMessage(uint32_t src, uint32_t dst, std::shared_ptr<BasicMsg> msg,
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
    Logger::msg("Sim.Progress", 0, real_time,
        " (", time, ", ", time.getRawDate() / 1000000.0 / real_duration, " sims/s)   ",
        numEvents, " ev (", numEvents / real_duration, " ev/s)   ",
        totalBytesSent, " trf (", numMsgSent, " msg, ", (double)totalBytesSent / numMsgSent, " B/msg, ",
        (totalBytesSent / (time.getRawDate() / 1000000.0)) / routingTable.size(), " Bps/node)   ",
        MemoryManager::getInstance().getUsedMemory(), " mem   100%");
    sstats.saveTotalStatistics();
    pstats.saveTotalStatistics();
    tstats.saveTotalStatistics();
    if (cs.get())
        cs->showStatistics();
}
