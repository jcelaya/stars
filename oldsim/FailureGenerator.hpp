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

#ifndef FAILUREGENERATOR_H_
#define FAILUREGENERATOR_H_

#include <vector>
#include "Simulator.hpp"

class FailureGenerator : public Simulator::InterEventHandler {
	double meanTime;
	unsigned int minFail, maxFail;
	unsigned int numFailing;
	unsigned int maxFailures;
	std::vector<unsigned int> failingNodes;

	void randomFailure();

public:
	FailureGenerator(double mtbf, unsigned int minf, unsigned int maxf, unsigned int mf = -1) :
			Simulator::InterEventHandler(), meanTime(mtbf), minFail(minf), maxFail(maxf), maxFailures(mf) {
		failingNodes.resize(sim.getNumNodes());
		for (unsigned int i = 0; i < failingNodes.size(); i++)
			failingNodes[i] = i;
		randomFailure();
	}

	bool blockEvent(const Simulator::Event & ev);
};

#endif /*FAILUREGENERATOR_H_*/
