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

#ifndef FAILUREGENERATOR_H_
#define FAILUREGENERATOR_H_

#include <vector>
#include "Time.hpp"
#include "Variables.hpp"
class BasicMsg;


/**
 * Node failure generation class.
 *
 * This class generates the failure of a node or set of nodes with a poisson process as described in
 * Rhea, S et. al. "Handling Churn in a DHT". Given the median session time tm of the nodes, failures are
 * generated with a poisson process of mean tm/(N * ln2), where N is the size of the network.
 * At each failure, more than one node may fail, and
 */
class FailureGenerator {
    double meanTime;
    DiscreteUniformVariable failNumVar;
    unsigned int numFailures, numFailed;
    std::vector<unsigned int> failingNodes;

    void programFailure(Duration failAt, unsigned int numFailing);

public:
    FailureGenerator() : failNumVar(0, 1) {}

    void startFailures(double median_session, unsigned int minf, unsigned int maxf);
    void bigFailure(Duration failAt, unsigned int minf, unsigned int maxf);

    bool isNextFailure(const BasicMsg & msg);

    unsigned int getNumFailures() const { return numFailures; }
};

#endif /*FAILUREGENERATOR_H_*/
