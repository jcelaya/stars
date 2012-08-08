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

#ifndef JOBSTATISTICS_H_
#define JOBSTATISTICS_H_

#include <list>
#include <utility>
#include <string>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Simulator.hpp"
#include "TaskDescription.hpp"
#include "Distributions.hpp"


class JobStatistics : public Simulator::InterEventHandler {
public:
	JobStatistics();
	~JobStatistics();

	void beforeEvent(const Simulator::Event & ev);

private:
	Histogram numNodesHist;
	Histogram finishedHist;
	Histogram searchHist;
	Histogram jttHist;
	Histogram seqHist;
	Histogram spupHist;
	Histogram slownessHist;
	unsigned int unfinishedJobs;
	unsigned int totalJobs;
	std::list<std::pair<Time, double> > lastSlowness;

	fs::ofstream jos;
	fs::ofstream ros;
	fs::ofstream sos;

	void finishApp(uint32_t node, long int appId, Time end, int finishedTasks);
};

#endif /*JOBSTATISTICS_H_*/
