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
#include <string>
#include <vector>
#include <boost/filesystem/fstream.hpp>
#include <stdexcept>
#include <log4cpp/Priority.hh>
#include <log4cpp/Category.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/FileAppender.hh>
#include "ConfigurationManager.hpp"
#include "Logger.hpp"
#include "StructureNode.hpp"
#include "ResourceNode.hpp"
#include "SubmissionNode.hpp"
#include "DPScheduler.hpp"
#include "DPDispatcher.hpp"
#include "config.h"
#ifdef Wt_FOUND
#include "WtUI.hpp"
#endif
using namespace log4cpp;
using namespace std;
namespace fs = boost::filesystem;


Appender * addConsoleLogging(Layout * layout) {
    OstreamAppender * console = new OstreamAppender("ConsoleAppender", &std::cout);
    if (layout == NULL) {
        PatternLayout * l = new PatternLayout();
        l->setConversionPattern("%d{%H:%M:%S.%l} %p %c : %m%n");
        layout = l;
    }
    console->setLayout(layout);
    Category::getRoot().addAppender(console);
    return console;
}


class BoostFileAppender : public LayoutAppender {
protected:
    virtual void _append(const LoggingEvent& event) {
        os << _getLayout().format(event);
        os.flush();
    }

    fs::ofstream os;

public:

    /**
     * Constructs a FileAppender.
     * @param name the name of the Appender.
     * @param fileName the name of the file to which the Appender has to log.
     * @param append whether the Appender has to truncate the file or
     * just append to it if it already exists. Defaults to 'true'.
     */
    BoostFileAppender(const string & name, const fs::path & fileName, bool append = true) :
            LayoutAppender(name) {
        os.open(fileName, ios_base::out | (append ? ios_base::app : ios_base::trunc));
    }

    virtual ~BoostFileAppender() {
        close();
    }

    void close() {
        os.close();
    }
};


Appender * addFileLogging(fs::path logFile, bool append, Layout * layout) {
    BoostFileAppender * file = new BoostFileAppender(logFile.string(), logFile, append);
    if (layout == NULL) {
        PatternLayout * l = new PatternLayout();
        l->setConversionPattern("%d{%H:%M:%S.%l} %p %c : %m%n");
        layout = l;
    }
    file->setLayout(layout);
    Category::getRoot().addAppender(file);
    return file;
}


int main(int argc, char * argv[]) {
    try {
        // Configure
        // Try to load default config file
        // TODO: check on /etc, the home dir, etc... also depending on the SO
        fs::path defaultConfigFile(".starsrc");
        if (fs::exists(defaultConfigFile))
            ConfigurationManager::getInstance().loadConfigFile(defaultConfigFile);
        // Command line overrides config file
        if (ConfigurationManager::getInstance().loadCommandLine(argc, argv)) return 0;
        // Start logging
        LogMsg::initLog(ConfigurationManager::getInstance().getLogConfig());
        addConsoleLogging(NULL);
        // Start io thread and listen to incoming connections
        CommLayer::getInstance().listen();
        // Get local address
        // Init CommLayer and standard services
        LogMsg("", DEBUG) << "Creating standard services";
//  CommLayer::getInstance().registerService(new StructureNode(2));
//  CommLayer::getInstance().registerService(ResourceNode(sn));
//  CommLayer::getInstance().registerService(SubmissionNode(rn));
//  CommLayer::getInstance().registerService(EDFScheduler(rn));
//  CommLayer::getInstance().registerService(DeadlineDispatcher(sn));
#ifdef Wt_FOUND
        // Start UI
        LogMsg("", DEBUG) << "Starting UI web server";
        WtUI::getInstance().start();
#endif
        LogMsg("", DEBUG) << "Starting main event loop";
        // Start event handling
        CommLayer::getInstance().commEventLoop();
        LogMsg("", DEBUG) << "Gracely exiting";
        return 0;
    } catch (std::exception & e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
}
