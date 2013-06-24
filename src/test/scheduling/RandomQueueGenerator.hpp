/*
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

#ifndef RANDOMQUEUEGENERATOR_HPP_
#define RANDOMQUEUEGENERATOR_HPP_

#include <list>
#include <boost/random/mersenne_twister.hpp>
#include "TaskProxy.hpp"

namespace stars {

class RandomQueueGenerator {
public:
    RandomQueueGenerator();

    RandomQueueGenerator(unsigned int s) : id(0) {
        seed(s);
    }

    void seed(unsigned int s);

    unsigned int getSeed() const { return theSeed; }

    std::list<TaskProxy> & createRandomQueue() {
        return createRandomQueue(getRandomPower());
    }

    std::list<TaskProxy> & createRandomQueue(double power);

    std::list<TaskProxy> & createNLengthQueue(unsigned int numTasks) {
        return createNLengthQueue(numTasks, getRandomPower());
    }

    std::list<TaskProxy> & createNLengthQueue(unsigned int numTasks, double power);

    double getRandomPower();

private:
    int getRandomAppLength();
    unsigned int getRandomNumTasks();
    double getRandomReleaseDelta();
    void createRandomApp(unsigned int numTasks);

    unsigned int theSeed;
    boost::random::mt19937 gen;
    unsigned int id;
    double currentPower;
    double currentrfirst;
    std::list<TaskProxy> currentTasks;
};

} // namespace stars

#endif /* RANDOMQUEUEGENERATOR_HPP_ */
