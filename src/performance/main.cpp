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
#include <vector>
#include <sstream>
#include <log4cpp/Category.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include "Logger.hpp"
#include "AggregationTest.hpp"
#include "util/MemoryManager.hpp"
#include "util/SignalException.hpp"
using namespace log4cpp;
using std::endl;

int main(int argc, char * argv[]) {
    std::vector<int> numClusters;
    int levels = 0;

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " num_levels clusters [clusters...]" << std::endl;
        return 1;
    }

    std::istringstream(argv[1]) >> levels;
    for (int i = 2; i < argc; ++i) {
        int clustersValue;
        std::istringstream(argv[i]) >> clustersValue;
        numClusters.push_back(clustersValue);
    }

    Logger::initLog("root=WARN");
    OstreamAppender * console = new OstreamAppender("ConsoleAppender", &std::cout);
    PatternLayout * l = new PatternLayout();
    l->setConversionPattern("%d{%H:%M:%S.%l} %p %c : %m%n");
    console->setLayout(l);
    Category::getRoot().addAppender(console);
    MemoryManager::getInstance().setUpdateDuration(0);
    SignalException::Handler::getInstance().setHandler();

    try {
        AggregationTest & t = AggregationTest::getInstance();
        t.performanceTest(numClusters, levels);
    } catch (SignalException & e) {
        std::cout << "foo";
        SignalException::Handler::getInstance().printStackTrace();
    }

    return 0;
}


Time Time::getCurrentTime() {
    return Time();
}
