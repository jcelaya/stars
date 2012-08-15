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
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test_log.hpp>
#include <boost/filesystem/fstream.hpp>
#include <map>
#include <vector>
#include <log4cpp/Category.hh>
#include <log4cpp/Priority.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/LayoutAppender.hh>
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <sstream>
#include "Time.hpp"
#include "TestHost.hpp"
#include "Scheduler.hpp"
#include "Logger.hpp"
using log4cpp::Category;
using log4cpp::Priority;
using boost::shared_ptr;


const boost::posix_time::ptime TestHost::referenceTime =
    boost::posix_time::ptime(boost::gregorian::date(2000, boost::gregorian::Jan, 1), boost::posix_time::seconds(0));


Time Time::getCurrentTime() {
    return TestHost::getInstance().getCurrentTime();
}


CommLayer & CommLayer::getInstance() {
    shared_ptr<CommLayer> & current = TestHost::getInstance().getCommLayer();
    if (!current.get()) {
        current.reset(new CommLayer);
        // Make io_service thread use same singleton instance
        //current->nm->io.dispatch(setInstance);
    }
    return *current;
}


ConfigurationManager & ConfigurationManager::getInstance() {
    shared_ptr<ConfigurationManager> & current = TestHost::getInstance().getConfigurationManager();
    if (!current.get())
        current.reset(new ConfigurationManager);
    // Set the working path
    current->setWorkingPath(current->getWorkingPath() / "test");
    // Set the memory database
    current->setDatabasePath(boost::filesystem::path(":memory:"));
    return *current;
}


// Print log messages through BOOST_TEST_MESSAGE
class TestAppender : public log4cpp::LayoutAppender {
public:
    TestAppender(const std::string & name) : log4cpp::LayoutAppender(name) {}

    void close() {}  // Do nothing

protected:
    void _append(const log4cpp::LoggingEvent & e) {
        std::ostringstream oss;
        oss << boost::this_thread::get_id() << ": " << _getLayout().format(e);
        BOOST_TEST_MESSAGE(oss.str());
    }
};


bool init_unit_test_suite() {
    // Clear all log priorities, root defaults to WARN
    std::vector<Category *> * cats = Category::getCurrentCategories();
    for (std::vector<Category *>::iterator it = cats->begin(); it != cats->end(); it++)
        (*it)->setPriority((*it)->getName() == std::string("") ? Priority::WARN : Priority::NOTSET);

    // Load log config file
    boost::filesystem::ifstream logconf(boost::filesystem::path("test")
            / boost::filesystem::path("share/test/PeerCompLibTest.logconf"));
    std::string line;
    if (getline(logconf, line).good()) {
        LogMsg::initLog(line);
    }
    // Test log priority is always DEBUG
    Category::getInstance("Test").setPriority(DEBUG);

    TestAppender * console = new TestAppender("ConsoleAppender");
    log4cpp::PatternLayout * layout = new log4cpp::PatternLayout();
    layout->setConversionPattern("%p %c : %m");
    console->setLayout(layout);
    Category::getRoot().setAdditivity(false);
    Category::getRoot().addAppender(console);

    return true;
}


int main(int argc, char * argv[]) {
    return boost::unit_test::unit_test_main(init_unit_test_suite, argc, argv);
}
