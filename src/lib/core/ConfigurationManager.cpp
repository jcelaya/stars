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

#include <iostream>
#include <sstream>
#include <boost/filesystem/fstream.hpp>
#include "ConfigurationManager.hpp"
#include "config.h"
using namespace std;
using namespace boost::program_options;
namespace fs = boost::filesystem;


ConfigurationManager & ConfigurationManager::getInstance() {
    static ConfigurationManager instance;
    return instance;
}


ConfigurationManager::ConfigurationManager() : description("Allowed options") {
    // Default values
    workingPath = boost::filesystem::initial_path();
    updateBW = 1000.0;
    slownessRatio = 2.0;
    port = 2030;
    uiPort = 2031;
#ifdef _DEBUG_
    logString = "root=DEBUG";
#else
    logString = "root=WARN";
#endif
    submitRetries = 3;
    heartbeat = 60;
    availMemory = 128;
    availDisk = 200;
    dbPath = workingPath / boost::filesystem::path("stars.db");
    requestTimeout = 30.0;

    // Options description
    description.add_options()
    ("log,l", value<string>(&logString), "logging configuration")
    ("port,p", value<uint16_t>(&port), "port for peer communication")
    ("ui_port", value<uint16_t>(&uiPort), "port for UI")
    ("mem,m", value<unsigned int>(&availMemory), "available memory for tasks")
    ("disk,d", value<unsigned int>(&availDisk), "available disk for tasks")
    ("update_bw,u", value<double>(&updateBW), "update bandwidth limit")
    ("retries,r", value<int>(&submitRetries), "automatic submission retries")
    ("heartbeat,h", value<int>(&heartbeat), "task heartbeat period")
    ;
}


CommAddress ConfigurationManager::getEntryPoint() const {
    if (entryPoint.empty()) return CommAddress("0.0.0.0", port);
    else {
        int sep = entryPoint.find_first_of(':');
        string entryAddress = entryPoint.substr(0, sep);
        uint16_t entryPort;
        istringstream(entryPoint.substr(sep + 1)) >> entryPort;
        return CommAddress(entryAddress, entryPort);
    }
}


void ConfigurationManager::loadConfigFile(fs::path configFile) {
    fs::ifstream fileStream(configFile);
    variables_map vm;
    store(parse_config_file(fileStream, description), vm);
    notify(vm);
}


bool ConfigurationManager::loadCommandLine(int argc, char * argv[]) {
    options_description cmd_line(description);
    cmd_line.add_options()
    ("config,c", value<string>(), "alternative configuration file")
    ("version,v", "print version string")
    ("help", "produce help message")
    ;

    variables_map vm;
    store(parse_command_line(argc, argv, cmd_line), vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << "Usage: stars-peer [options]" << endl;
        cerr << cmd_line << endl;
        return true;
    }
    if (vm.count("version")) {
        cerr << "STaRS peer v" << STARS_VERSION_MAJOR << '.' << STARS_VERSION_MINOR << endl;
        cerr << "Copyright Javier Celaya, Universidad de Zaragoza. Licensed under GPLv3." << endl;
        return true;
    }
    if (vm.count("config")) {
        fs::path configFile(vm["config"].as<string>());
        if (fs::exists(configFile)) {
            loadConfigFile(configFile);
        } else {
            cerr << "Config file not found: " << configFile;
        }
    }

    return false;
}
