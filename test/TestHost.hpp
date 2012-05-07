/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2011 Javier Celaya
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

#ifndef TESTHOST_HPP_
#define TESTHOST_HPP_

#include <boost/thread/tss.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <list>
#include <utility>
#include "ConfigurationManager.hpp"
#include "CommLayer.hpp"


// Fake singleton, allows to reset the CommLayer and ConfigurationManager instances when necessary
class TestHost {
	struct Host {
		boost::shared_ptr<CommLayer> commLayer;
		boost::shared_ptr<ConfigurationManager> confMngr;
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

	boost::shared_ptr<CommLayer> & getCommLayer() {
		if (!myHost.get()) myHost.reset(new std::list<Host>::iterator(hosts.begin()));
		return (*myHost)->commLayer;
	}

	boost::shared_ptr<ConfigurationManager> & getConfigurationManager() {
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
