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
#include <sstream>
#include <algorithm>
#include <log4cpp/Category.hh>
#include <boost/iostreams/filter/gzip.hpp>
#include <xbt/log.h>
#include "Logger.hpp"
#include "ConfigurationManager.hpp"
#include "Simulator.hpp"
#include "SimulationCase.hpp"
#include "SimTask.hpp"
#include "MemoryManager.hpp"
#include "portable_binary_oarchive.hpp"
namespace fs = boost::filesystem;
namespace pt = boost::posix_time;


#define PROGRESS(x) {std::ostringstream foo; foo << x; progressLog(foo.str().c_str());}

XBT_LOG_NEW_CATEGORY(Sim, "Simulation messages");
XBT_LOG_NEW_DEFAULT_SUBCATEGORY(Progress, Sim, "Simulation progress messages");


namespace {
    int * createPriorityTable() {
        static int table[NOTSET + 1];
        for (int i = 0; i < ERROR; ++i) table[i] = xbt_log_priority_critical;
        for (int i = ERROR; i < WARN; ++i) table[i] = xbt_log_priority_error;
        for (int i = WARN; i < NOTICE; ++i) table[i] = xbt_log_priority_warning;
        for (int i = NOTICE; i < INFO; ++i) table[i] = xbt_log_priority_info;
        for (int i = INFO; i < DEBUG; ++i) table[i] = xbt_log_priority_verbose;
        for (int i = DEBUG; i <= NOTSET; ++i) table[i] = xbt_log_priority_debug;
        return table;
    }
    int * log4cpp2XBTPrio = createPriorityTable();
}


// TODO: log with XBT
void LogMsg::setPriority(const std::string & catPrio) {
//     int pos = catPrio.find_first_of('=');
//     if (pos == (int)std::string::npos) return;
//     std::string category = catPrio.substr(0, pos);
//     std::string priority = catPrio.substr(pos + 1);
//     try {
//         log4cpp::Priority::Value p = log4cpp::Priority::getPriorityValue(priority);
//         if (category == "root") {
//             log4cpp::Category::setRootPriority(p);
//         } else {
//             log4cpp::Category::getInstance(category).setPriority(p);
//         }
//     } catch (std::invalid_argument & e) {}
}


void LogMsg::log(const std::string & category, int priority, LogMsg::AbstractTypeContainer * values) {
    Simulator::getInstance().log(category, log4cpp2XBTPrio[priority], values);
}

// TODO: log with XBT
void Simulator::log(const std::string & category, int priority, LogMsg::AbstractTypeContainer * values) {
    if (debugFile.is_open() && log4cpp::Category::getInstance(category).isPriorityEnabled(priority)) {
        debugArchive << Duration(getRealTime().total_microseconds()) << ' ' << getCurrentTime() << ' '
                << getCurrentNode().getLocalAddress() << ','
                << category << '(' << priority << ')' << ' ';
        for (LogMsg::AbstractTypeContainer * it = values; it != NULL; it = it->next)
            debugArchive << *it;
        debugArchive << std::endl;
    }
}

void Simulator::progressLog(const char * msg) {
    XBT_CLOG(Progress, xbt_log_priority_critical, "#%d: %s", getpid(), msg);
    
    if (progressFile.is_open()) {
        progressFile << msg << std::endl;
    }
}


void finish(int param) {
    std::cout << "Stopping due to user signal" << std::endl;
    MSG_process_killall(-1);
}


int main(int argc, char * argv[]) {
#ifdef __x86_64__
    std::cout << "STaRS SimGrid-based simulator 64bits PID " << getpid() << std::endl;
#else
    std::cout << "STaRS SimGrid-based simulator 32bits PID " << getpid() << std::endl;
#endif
    if (argc != 2) {
        std::cout << "Usage: stars-sim config_file" << std::endl;
        return 1;
    }

    // Avoid huge core dumps on error
#ifndef WITH_CORE_DUMP
    rlimit zeroLimit;
    zeroLimit.rlim_cur = zeroLimit.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &zeroLimit);
#endif

    MemoryManager::getInstance().reset();
    Simulator & sim = Simulator::getInstance();
    std::signal(SIGUSR1, finish);
    
    // Init simulation framework
    MSG_global_init(&argc, argv);
    MSG_set_channel_number(1);

    Properties property;
    property.loadFromFile(std::string(argv[1]));
    bool result = sim.run(property);
    
    // Cleanup
    MSG_clean();
    return result ? 0 : 1;
}


static bool checkLogFile(const fs::path & logFile) {
    fs::ifstream ifs(logFile);
    if (!ifs.good()) {
        return false;
    }
    ifs.seekg(0, std::ios::end);
    ifs.unget();
    while (ifs.good() && ifs.peek() != '\n') ifs.unget();
    ifs.unget();
    while (ifs.good() && ifs.peek() != '\n') ifs.unget();
    ifs.ignore(1);
    std::string line;
    getline(ifs, line);
    return line.find("Ending test at") != std::string::npos;
}


bool Simulator::run(Properties & property) {
    pt::ptime start = pt::microsec_clock::local_time();
    
    boost::shared_ptr<SimulationCase> simCase = CaseFactory::getInstance().createCase(property("case_name", std::string("")), property);
    if (!simCase.get()) {
        PROGRESS("ERROR: No test exists with name \"" << property("case_name", std::string("")) << '"');
        return false;
    }
    
    //pstats.startEvent("Prepare simulation case");
    
    // Set logging
    resultDir = property("results_dir", std::string("./results"));
    if (!fs::exists(resultDir)) fs::create_directories(resultDir);
    fs::path logFile("execution.log");
    if (fs::exists(resultDir / logFile) && !property("overwrite", false) && checkLogFile(resultDir / logFile)) {
        PROGRESS("Log file exists at " << (resultDir / logFile));
        return true;
    }
    PROGRESS("Logging to " << (resultDir / logFile));

    progressFile.open(resultDir / logFile);
    debugFile.open(resultDir / "debug.log");
    if (debugFile.is_open()) {
        debugArchive.push(boost::iostreams::gzip_compressor());
        debugArchive.push(debugFile);
    }
    xbt_log_control_set(property("log_conf_string", std::string("")).c_str());
    PROGRESS("Running simulation test at " << pt::microsec_clock::local_time() << ": " << property);

    //pstats.openFile(resultDir);
    
    // Simulation variables
    maxRealTime = pt::seconds(property("max_time", 0));
    maxSimTime = Duration(property("max_sim_time", 0.0));
    maxMemUsage = property("max_mem", 0U);
    srand(property("seed", 12345));
    showStep = property("show_step", 100000);

    // Library config
    ConfigurationManager::getInstance().setUpdateBandwidth(property("update_bw", 1000.0));
    ConfigurationManager::getInstance().setStretchRatio(property("stretch_ratio", 2.0));
    ConfigurationManager::getInstance().setHeartbeat(property("heartbeat", 300));
    ConfigurationManager::getInstance().setWorkingPath(resultDir);

    nodeFactory.setupFactory(property);
    platformFile = property("platform_file", std::string());
    MSG_create_environment(platformFile.c_str());
    m_host_t * hosts = MSG_get_host_table();
    int numNodes = MSG_get_host_number();
    routingTable.resize(numNodes);
    // For every host in the platform, set Simulator::processFunction as the process function
    for (int i = 0; i < numNodes; ++i) {
        MSG_host_set_data(hosts[i], &routingTable[i]);
        MSG_process_create(NULL, Simulator::processFunction, NULL, hosts[i]);
    }
    
    PROGRESS(MemoryManager::getInstance().getMaxUsedMemory() << " bytes to prepare simulation network.");
    
    simCase->preStart();
    //pstats.endEvent("Prepare simulation case");
    
    // Run simulation
    simStart = pt::microsec_clock::local_time();
    MSG_error_t res = MSG_main();

    simCase->postEnd();
    // Show statistics
    pt::ptime end = pt::microsec_clock::local_time();
    pt::time_duration simRealTime = end - simStart;
    PROGRESS("" << simRealTime << " (" << getCurrentTime() << ", " << (getCurrentTime().getRawDate() / simRealTime.total_microseconds()) << " speedup)   "
        << MemoryManager::getInstance().getUsedMemory() << " mem   100%");
    //pcstats.saveTotalStatistics();
    //pstats.saveTotalStatistics();
    PROGRESS("Ending test at " << end << ". Lasted " << (end - start)
        << " and used " << MemoryManager::getInstance().getMaxUsedMemory() << " bytes.");
    
    return res == MSG_OK;
}


//void Simulator::run() {
//     while (!events.empty() && !doStop && simCase.doContinue()) {
//         if (maxEvents && numEvents >= maxEvents) {
//             LogMsg("Sim.Progress", 0) << "Maximum number of events limit reached: " << maxEvents;
//             break;
//         } else if (maxRealTime > seconds(0) && microsec_clock::local_time() - realStart >= maxRealTime) {
//             LogMsg("Sim.Progress", 0) << "Maximum real time limit reached: " << maxRealTime;
//             break;
//         } else if (maxSimTime > Duration(0.0) && time - Time() >= maxSimTime) {
//             LogMsg("Sim.Progress", 0) << "Maximum simulation time limit reached: " << maxSimTime;
//             break;
//         } else if (maxMemUsage && numEvents % 1000 == 0 && (MemoryManager::getInstance().getMaxUsedMemory() >> 20) > maxMemUsage) {
//             LogMsg("Sim.Progress", 0) << "Maximum memory usage limit reached: " << maxMemUsage;
//             break;
//         }
//         stepForward();
//         if (numEvents % showStep == 0) {
//             end = microsec_clock::local_time();
//             real_time += end - start;
//             double real_duration = (end - start).total_microseconds() / 1000000.0;
//             start = end;
//             // Show statistics
//             LogMsg("Sim.Progress", 0) << real_time << " (" << time << ")   "
//             << numEvents << " ev (" << (showStep / real_duration) << " ev/s)   "
//             << MemoryManager::getInstance().getUsedMemory() << " mem   "
//             << simCase.getCompletedPercent() << "%   "
//             << SimTask::getRunningTasks() << " tasks";
//             pstats.savePartialStatistics();
//         }
//     }
//}


int Simulator::processFunction(int argc, char * argv[]) {
    // Initialise the current node with:
    //   - Data from the simulation case properties
    //   - Its local address
    //   - Its m_host_t value
    Simulator & sim = Simulator::getInstance();
    PeerCompNode & node = getCurrentNode();
    uint32_t local = &node - &sim.routingTable[0];
    sim.nodeFactory.setupNode(local, node);
    // Start its message loop
    node.mainLoop();
}


unsigned long int Simulator::getMsgSize(boost::shared_ptr<BasicMsg> msg) {
    unsigned long int size = 0;
    try {
        std::stringstream ss;
        portable_binary_oarchive oa(ss);
        BasicMsg * tmp = msg.get();
        oa << tmp;
        size = ss.tellp();
    } catch (...) {
        PROGRESS("Error serializing message of type " << msg->getName());
    }
    return size;
}


unsigned int Simulator::sendMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg) {
    // NOTE: msg must not be clonned, to track messages.

    // Measure operation duration, only if inEvent().
    //Duration opDuration(inEvent() ? (microsec_clock::local_time() - opStart).total_microseconds() : 0.0);

    unsigned long int size = src != dst ? getMsgSize(msg) + 90 : 0;
    return size;
}


unsigned int Simulator::injectMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg,
                                      Duration d, bool withOpDuration) {
    // NOTE: msg must not be clonned, to track messages.

    unsigned long int size = getMsgSize(msg);
    if (withOpDuration)
        d += Duration((pt::microsec_clock::local_time() - opStart).total_microseconds());
    return size;
}


int Simulator::setTimer(uint32_t dst, Time when, boost::shared_ptr<BasicMsg> msg) {
}


void Simulator::cancelTimer(int timerId) {
}
