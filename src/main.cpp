#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "ConfigurationManager.hpp"
#include "config.h"
namespace fs = boost::filesystem;


int main(int argc, char **argv) {
    // Configure
    // Try to load default config file
    // TODO: check on /etc, the home dir, etc... also depending on the SO
    fs::path defaultConfigFile(".peercomprc");
    if (fs::exists(defaultConfigFile))
            ConfigurationManager::getInstance().loadConfigFile(defaultConfigFile);
    // Command line overrides config file
    if(ConfigurationManager::getInstance().loadCommandLine(argc, argv)) return 0;
    // Start logging
    //initLog(ConfigurationManager::getInstance().getLogConfig());
    //addConsoleLogging(NULL);
    std::cout << "STaRS version " << STARS_VERSION_MAJOR << '.' << STARS_VERSION_MINOR << std::endl;
    // Start io thread and listen to incoming connections
    //CommLayer::getInstance().listen();
    // Get local address
    // Init CommLayer and standard services
    //LogMsg("", DEBUG) << "Creating standard services";
    //              StructureNode sn(2);
    //              ResourceNode rn(sn);
    //              SubmissionNode sbn(rn);
    //              EDFScheduler sch(rn);
    //              DeadlineDispatcher tbd(sn);
    // Start UI
    //LogMsg("", DEBUG) << "Starting UI web server";
    //WtUI::getInstance().start();
    //LogMsg("", DEBUG) << "Starting main event loop";
    // Start event handling
    //CommLayer::getInstance().commEventLoop();
    //LogMsg("", DEBUG) << "Gracely exiting";
    return 0;
}
