/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
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

#ifndef SLOWNESSSTATISTICS_H_
#define SLOWNESSSTATISTICS_H_

#include <string>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Simulator.hpp"


class SlownessStatistics : public Simulator::InterEventHandler {
	fs::ofstream os;

	std::vector<double> slowness;
	std::vector<double *> slownessDesc;
	int lastNonZero;

public:
	SlownessStatistics();

	void afterEvent(const Simulator::Event & ev);
};

#endif /* SLOWNESSSTATISTICS_H_ */
