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

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "TaskProxy.hpp"

namespace stars {

class RandomQueueGenerator {
public:
    static RandomQueueGenerator & getInstance() {
        static RandomQueueGenerator rqg;
        return rqg;
    }

    void seed(unsigned int s) { gen.seed(s); }

    TaskProxy::List createRandomQueue() {
        TaskProxy::List result;
        Time now = TestHost::getInstance().getCurrentTime();
        double power = getRandomPower();

        // Add a random number of applications, with random length and number of tasks
        // At least one app
        unsigned int numTasks = getRandomNumTasks();
        int a = getRandomAppLength() / numTasks;
        // Get a release date so that the first task is still executing
        double rfirst = getRandomReleaseDelta(-a / power);
        for (unsigned int taskid = 0; taskid < numTasks; ++taskid) {
            result.push_back(TaskProxy(a, power, now + Duration(rfirst)));
            result.back().id = id++;
        }

        for(int appid = 0; boost::random::uniform_int_distribution<>(1, 3)(gen) != 1; ++appid) {
            // Get a release date after the first app
            double r = getRandomReleaseDelta(rfirst);
            numTasks = getRandomNumTasks();
            a = getRandomAppLength() / numTasks;
            for (unsigned int taskid = 0; taskid < numTasks; ++taskid) {
                result.push_back(TaskProxy(a, power, now + Duration(r)));
                result.back().id = id++;
            }
        }

        return result;
    }

    TaskProxy::List createNLengthQueue(int n) {
        TaskProxy::List result;
        Time now = TestHost::getInstance().getCurrentTime();
        double power = getRandomPower();

        // At least one app
        int a = getRandomAppLength();
        // Get a release date so that the first task is still executing
        double rfirst = getRandomReleaseDelta(-a / power);
        result.push_back(TaskProxy(a, power, now + Duration(rfirst)));
        result.back().id = id++;
        result.back().t += rfirst;

        // Add n tasks with random length
        for(int appid = 0; appid < n; ++appid) {
            double r = getRandomReleaseDelta(rfirst);
            // Applications between 1-4h on a 1000 MIPS computer
            int a = getRandomAppLength();
            result.push_back(TaskProxy(a, power, now + Duration(r)));
            result.back().id = id++;
        }

        return result;
    }

private:
    // Applications between 1-4h on a 1000 MIPS computer
    int getRandomAppLength() { return boost::random::uniform_int_distribution<>(600000, 14400000)(gen); }
    int getRandomNumTasks() { return boost::random::uniform_int_distribution<>(1, 10)(gen); }
    double getRandomPower() { return floor(boost::random::uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200; }
    double getRandomReleaseDelta(int min) { return boost::random::uniform_int_distribution<>(min, 0)(gen); }

    boost::random::mt19937 gen;
    unsigned int id;

    RandomQueueGenerator() : id(0) {}
};

} // namespace stars

#endif /* RANDOMQUEUEGENERATOR_HPP_ */
