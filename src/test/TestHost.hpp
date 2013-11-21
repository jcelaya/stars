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

#ifndef TESTHOST_HPP_
#define TESTHOST_HPP_

#include <boost/thread/tss.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <list>
#include <utility>
#include "ConfigurationManager.hpp"
#include "CommLayer.hpp"


// Fake singleton, allows to reset the CommLayer and ConfigurationManager instances when necessary
class TestHost {
    struct Host {
        std::shared_ptr<CommLayer> commLayer;
        std::shared_ptr<ConfigurationManager> confMngr;
        Time currentTime;
        bool realTime;
        Host() : currentTime(referenceTime), realTime(false) {}
    };

    std::list<Host> hosts;
    boost::thread_specific_ptr<std::list<Host>::iterator> myHost;

    TestHost() {}

    static const boost::posix_time::ptime referenceTime;

public:
    static TestHost & getInstance() {
        static TestHost instance;
        return instance;
    }

    void addSingleton() {
        hosts.push_front(Host());
        myHost.reset(new std::list<Host>::iterator(hosts.begin()));
    }

    std::shared_ptr<CommLayer> & getCommLayer() {
        if (!myHost.get()) myHost.reset(new std::list<Host>::iterator(hosts.begin()));
        return (*myHost)->commLayer;
    }

    std::shared_ptr<ConfigurationManager> & getConfigurationManager() {
        if (!myHost.get()) myHost.reset(new std::list<Host>::iterator(hosts.begin()));
        return (*myHost)->confMngr;
    }

    Time getCurrentTime() {
        if (!myHost.get()) myHost.reset(new std::list<Host>::iterator(hosts.begin()));
        if ((*myHost)->realTime)
            return Time((boost::posix_time::microsec_clock::universal_time() - referenceTime).total_microseconds());
        else
            return (*myHost)->currentTime;
    }

    void setCurrentTime(Time t) {
        if (!myHost.get()) myHost.reset(new std::list<Host>::iterator(hosts.begin()));
        (*myHost)->currentTime = t;
    }

    void setRealTimeClock(bool c) {
        if (!myHost.get()) myHost.reset(new std::list<Host>::iterator(hosts.begin()));
        (*myHost)->realTime = c;
    }

    void reset() {
        hosts.clear();
        addSingleton();
    }
};


#endif /* TESTHOST_HPP_ */
