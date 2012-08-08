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

#ifndef PERFORMANCESTATISTICS_H_
#define PERFORMANCESTATISTICS_H_

#include <map>
#include <string>
#include <ctime>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;

class PerformanceStatistics {
public:
	struct EventStats {
		clock_t start;
		unsigned long int totalNumEvents, partialNumEvents;
		double totalHandleTime, partialHandleTime;
		EventStats() : totalNumEvents(0), partialNumEvents(0), totalHandleTime(0.0), partialHandleTime(0.0) {}
	};

	static const double clocksPerUSecond = CLOCKS_PER_SEC / 1000000.0;
private:
	std::map<std::string, EventStats> handleTimeStatistics;

	fs::ofstream os;

public:
	void openFile(const fs::path & statDir);

	void startEvent(const std::string & ev);

	void endEvent(const std::string & ev);

	EventStats getEvent(const std::string & ev) const;

	void savePartialStatistics();

	void saveTotalStatistics();
};

#endif /* PERFORMANCESTATISTICS_H_ */
