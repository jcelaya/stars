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
class BasicMsg;

class FailureGenerator {
    double meanTime;
    unsigned int minFail, maxFail;
    unsigned int numFailing;
    unsigned int maxFailures;
    std::vector<unsigned int> failingNodes;

    void randomFailure();

public:
    void startFailures(double mtbf, unsigned int minf, unsigned int maxf, unsigned int mf = -1);

    bool isNextFailure(const BasicMsg & msg);
};

#endif /*FAILUREGENERATOR_H_*/
