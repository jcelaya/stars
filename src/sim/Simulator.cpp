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
#include <sstream>
#include <log4cpp/Category.hh>
#include <boost/iostreams/filter/gzip.hpp>
#include <xbt/log.h>
#include "config.h"
#include "Logger.hpp"
#include "Simulator.hpp"
#include "SimulationCase.hpp"
#include "SimTask.hpp"
#include "MemoryManager.hpp"
namespace fs = boost::filesystem;
namespace pt = boost::posix_time;


XBT_LOG_NEW_DEFAULT_CATEGORY(Progress, "Simulation progress messages");
#define PROGRESS(x) {std::ostringstream foo; foo << x; XBT_CRITICAL("%s", foo.str().c_str());}


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


void LogMsg::log(const char * category, int priority, LogMsg::AbstractTypeContainer * values) {
    Simulator::getInstance().log(category, priority, values);
}


void Simulator::log(const char * category, int priority, LogMsg::AbstractTypeContainer * values) {
    if (debugFile.is_open() && log4cpp::Category::getInstance(category).isPriorityEnabled(priority)) {
        boost::mutex::scoped_lock lock(debugMutex);
        if (inSimulation)
            debugArchive << Duration(getRealTime().total_microseconds()) << ' ' << Time::getCurrentTime() << ' ' << getCurrentNode().getLocalAddress() << ',';
        else
            debugArchive << "sim_init,";
        debugArchive << category << '(' << priority << ')' << ' ';
        for (LogMsg::AbstractTypeContainer * it = values; it != NULL; it = it->next)
            debugArchive << *it;
        debugArchive << std::endl;
    }
}


void finish(int param) {
    XBT_CRITICAL("Stopping due to user signal");
    Simulator::getInstance().stop();
}


void showInformation(int param) {
    Simulator::getInstance().showInformation();
    std::signal(SIGUSR1, showInformation);
}


int main(int argc, char * argv[]) {
    xbt_log_control_set("root.fmt:%m%n");
#ifdef __x86_64__
    int bits = 64;
#else
    int bits = 32;
#endif
    XBT_CRITICAL("STaRS v%s.%s SimGrid-based simulator %dbits PID %d", STARS_VERSION_MAJOR, STARS_VERSION_MINOR, bits, getpid());
    if (argc < 2) {
        XBT_CRITICAL("Usage: stars-sim config_file");
        return 1;
    }

    // Avoid huge core dumps on error
#ifndef WITH_CORE_DUMP
    rlimit zeroLimit;
    zeroLimit.rlim_cur = zeroLimit.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &zeroLimit);
#endif

    std::signal(SIGUSR1, showInformation);
    std::signal(SIGINT, finish);
    std::signal(SIGTERM, finish);

    // Init simulation framework
    MSG_global_init(&argc, argv);

    bool result = Simulator::getInstance().run(std::string(argv[1]));

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


bool Simulator::run(const std::string & confFile) {
    Properties property;
    if (confFile == "-")
        property.loadFrom(std::cin);
    else {
        fs::ifstream ifs(confFile);
        property.loadFrom(ifs);
    }

    // Simulator starts running
    pt::ptime start = pt::microsec_clock::local_time();
    inSimulation = false;
    currentNode = 0;

    // Create simulation case
    simCase.reset(CaseFactory::getInstance().createCase(property("case_name", std::string("")), property));
    if (!simCase.get()) {
        XBT_CRITICAL("ERROR: No test exists with name \"%s\"", property("case_name", std::string("")).c_str());
        return false;
    }
    end = false;

    // Prepare simulation case
    pstats.resizeNumNodes(1);
    pstats.startEvent("Prepare simulation case");

    // Get working directory, and check if the execution log exists and/or must be overwritten
    resultDir = property("results_dir", std::string("./results"));
    if (!fs::exists(resultDir)) fs::create_directories(resultDir);
    fs::path logFile(resultDir / "execution.log");
    if (fs::exists(logFile) && !property("overwrite", false) && checkLogFile(logFile)) {
        XBT_CRITICAL("Log file exists at %s", logFile.c_str());
        return true;
    }
    pstats.openStatsFile();
    starsStats.openStatsFiles();

    // Set logging
    xbt_log_appender_set(&_simgrid_log_category__Progress, xbt_log_appender_file_new(const_cast<char *>(logFile.c_str())));
    xbt_log_layout_set(&_simgrid_log_category__Progress, xbt_log_layout_format_new(const_cast<char *>("%m%n")));
    xbt_log_additivity_set(&_simgrid_log_category__Progress, 1);
    debugFile.open(resultDir / "debug.log.gz");
    if (debugFile.is_open()) {
        debugArchive.push(boost::iostreams::gzip_compressor());
        debugArchive.push(debugFile);
    }
    LogMsg::initLog(property("log_conf_string", std::string("root=WARN")));
    PROGRESS("Running simulation test at " << pt::microsec_clock::local_time() << ": " << property);

    // Simulation variables
    maxRealTime = pt::seconds(property("max_time", 0));
    maxSimTime = Duration(property("max_sim_time", 0.0));
    maxMemUsage = property("max_mem", 0U);
    srand(property("seed", 12345));
    showStep = property("show_step", 5);

    // Library config
    StarsNode::libStarsConfigure(property);

    // Platform setup
    MSG_create_environment(fs::path(property("platform_file", std::string())).native().c_str());
    xbt_dynar_t hosts = MSG_hosts_as_dynar();
    numNodes = xbt_dynar_length(hosts);
    pstats.resizeNumNodes(numNodes);
    routingTable.reset(new StarsNode[numNodes]);
    // For every host in the platform, set StarsNode::processFunction as the process function
    for (currentNode = 0; currentNode < numNodes; ++currentNode) {
        m_host_t host = xbt_dynar_get_as(hosts, currentNode, m_host_t);
        MSG_host_set_data(host, &routingTable[currentNode]);
        routingTable[currentNode].setup(currentNode, host);
        MSG_process_create(NULL, StarsNode::processFunction, NULL, host);
    }

    simCase->preStart();

    PROGRESS(numNodes << " nodes, " << MemoryManager::getInstance().getMaxUsedMemory() << " bytes to prepare simulation network");
    pstats.endEvent("Prepare simulation case");

    // Run simulation
    simStart = pt::microsec_clock::local_time();
    nextProgress = simStart + pt::seconds(showStep);
    inSimulation = true;
    MSG_error_t res = MSG_main();
    inSimulation = false;

    simCase->postEnd();
    // Show statistics
    pt::ptime now = pt::microsec_clock::local_time();
    pt::time_duration simRealTime = now - simStart;
    PROGRESS(simRealTime << " (" << Time::getCurrentTime() << ", "
        << (Time::getCurrentTime().getRawDate() / simRealTime.total_microseconds()) << " speedup)   "
        << MemoryManager::getInstance().getUsedMemory() << " mem   100%");
    starsStats.saveTotalStatistics();
    pstats.saveTotalStatistics();

    // Finish
    for (currentNode = 0; currentNode < numNodes; ++currentNode) {
        routingTable[currentNode].finish();
    }

    PROGRESS("Ending test at " << now << ". Lasted " << (now - start)
        << " and used " << MemoryManager::getInstance().getMaxUsedMemory() << " bytes.");

    return res == MSG_OK;
}


void Simulator::showInformation() {
    PROGRESS(simCase->getProperties());
}


bool Simulator::doContinue() {
    if (end) return false;

    {
        boost::mutex::scoped_lock lock(stepMutex);
        pt::ptime now = pt::microsec_clock::local_time();
        if (maxSimTime > Duration(0.0) && Duration(MSG_get_clock()) > maxSimTime) {
            PROGRESS("Maximum simulation time limit reached: " << maxSimTime);
            end = true;
        } else if (maxRealTime > pt::seconds(0) && now - simStart >= maxRealTime) {
            PROGRESS("Maximum real time limit reached: " << maxRealTime);
            end = true;
        } else if (maxMemUsage && (MemoryManager::getInstance().getMaxUsedMemory() >> 20) > maxMemUsage) {
            XBT_CRITICAL("Maximum memory usage limit reached: %d", maxMemUsage);
            end = true;
        }
        if (!end && now >= nextProgress) {
            // Show statistics
            pt::time_duration simRealTime = now - simStart;
            PROGRESS(simRealTime << " (" << Time::getCurrentTime() << ", "
                << (Time::getCurrentTime().getRawDate() / simRealTime.total_microseconds()) << " speedup)   "
                << MemoryManager::getInstance().getUsedMemory() << " mem   "
                << simCase->getCompletedPercent() << "%   "
                << starsStats.getExistingTasks() << " tasks");
            pstats.savePartialStatistics();
            nextProgress = now + pt::seconds(showStep);
        }
    }

    return !end;
}
