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

#include "Task.hpp"

namespace stars {

class RandomQueueGenerator {
public:
    RandomQueueGenerator();

    RandomQueueGenerator(unsigned int s) {
        seed(s);
    }

    void seed(unsigned int s);

    unsigned int getSeed() const { return theSeed; }

    std::list<std::shared_ptr<Task> > & createRandomQueue() {
        return createRandomQueue(getRandomPower());
    }

    std::list<std::shared_ptr<Task> > & createRandomQueue(double power);

    std::list<std::shared_ptr<Task> > & createNLengthQueue(unsigned int numTasks) {
        return createNLengthQueue(numTasks, getRandomPower());
    }

    std::list<std::shared_ptr<Task> > & createNLengthQueue(unsigned int numTasks, double power);

    double getRandomPower();

    boost::random::mt19937 & getGenerator() { return gen; }

private:
    int getRandomAppLength();
    unsigned int getRandomNumTasks();
    double getRandomReleaseDelta();
    void createRandomApp(unsigned int numTasks);
    void reset(double power);

    unsigned int theSeed;
    boost::random::mt19937 gen;
    unsigned int appId;
    double currentPower;
    double currentrfirst;
    double tsum;
    std::list<std::shared_ptr<Task> > currentTasks;
};

} // namespace stars

#endif /* RANDOMQUEUEGENERATOR_HPP_ */
