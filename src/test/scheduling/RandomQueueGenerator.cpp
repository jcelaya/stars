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

#include <ctime>
#include <boost/random/uniform_int_distribution.hpp>
#include "RandomQueueGenerator.hpp"
#include "TestHost.hpp"
#include "Logger.hpp"

namespace stars {

void RandomQueueGenerator::seed(unsigned int s) {
    LogMsg("Test.RQG", DEBUG) << "Using seed " << s;
    theSeed = s;
    gen.seed(s);
}


std::list<TaskProxy> & RandomQueueGenerator::createRandomQueue(double power) {
    currentPower = power;
    currentTasks.clear();

    // Add a random number of applications, with random length and number of tasks
    while(boost::random::uniform_int_distribution<>(1, 3)(gen) != 1) {
        createRandomApp(getRandomNumTasks());
    }

    return currentTasks;
}


std::list<TaskProxy> & RandomQueueGenerator::createNLengthQueue(unsigned int numTasks, double power) {
    currentPower = power;
    currentTasks.clear();

    // Add n tasks with random length
    for(unsigned int appid = 0; appid < numTasks; ++appid) {
        createRandomApp(1);
    }

    return currentTasks;
}


void RandomQueueGenerator::createRandomApp(unsigned int numTasks) {
    Time now = TestHost::getInstance().getCurrentTime();
    int a = getRandomAppLength() / numTasks;
    double alreadyExecuted = 0.0;
    double r;
    if (currentTasks.empty()) {
        // Get a release date so that the first task is still executing
        currentrfirst = -a / currentPower;
        currentrfirst = alreadyExecuted = r = getRandomReleaseDelta();
    } else {
        r = getRandomReleaseDelta();
    }
    for (unsigned int taskid = 0; taskid < numTasks; ++taskid) {
        currentTasks.push_back(TaskProxy(a, currentPower, now + Duration(r)));
        currentTasks.back().id = id++;
    }
    currentTasks.front().t += alreadyExecuted;
}


// Applications between 1-4h on a 1000 MIPS computer
int RandomQueueGenerator::getRandomAppLength() {
    return boost::random::uniform_int_distribution<>(600000, 14400000)(gen);
}


unsigned int RandomQueueGenerator::getRandomNumTasks() {
    return boost::random::uniform_int_distribution<>(1, 10)(gen);
}


double RandomQueueGenerator::getRandomPower() {
    return floor(boost::random::uniform_int_distribution<>(1000, 3000)(gen) / 200.0) * 200;
}


double RandomQueueGenerator::getRandomReleaseDelta() {
    return boost::random::uniform_int_distribution<>(currentrfirst, 0)(gen);
}


RandomQueueGenerator::RandomQueueGenerator() : id(0) {
    seed(std::time(NULL));
}

} // namespace stars
