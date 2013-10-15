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
#include "Logger.hpp"
#include "TestTask.hpp"

namespace stars {

void RandomQueueGenerator::seed(unsigned int s) {
    LogMsg("Test.RQG", DEBUG) << "Using seed " << s;
    theSeed = s;
    gen.seed(s);
}


void RandomQueueGenerator::reset(double power) {
    currentPower = power;
    currentTasks.clear();
    appId = 0;
    tsum = 0;
}


std::list<boost::shared_ptr<Task> > & RandomQueueGenerator::createRandomQueue(double power) {
    reset(power);

    // Add a random number of applications, with random length and number of tasks
    while(boost::random::uniform_int_distribution<>(1, 8)(gen) != 1) {
        createRandomApp(getRandomNumTasks());
    }

    return currentTasks;
}


std::list<boost::shared_ptr<Task> > & RandomQueueGenerator::createNLengthQueue(unsigned int numTasks, double power) {
    reset(power);

    // Add n tasks with random length
    for(unsigned int appid = 0; appid < numTasks; ++appid) {
        createRandomApp(1);
    }

    return currentTasks;
}


void RandomQueueGenerator::createRandomApp(unsigned int numTasks) {
    Time now = Time::getCurrentTime();
    int a = getRandomAppLength() / numTasks;
    Duration alreadyExecuted(0.0);
    double r;
    Time endtime = now;
    TaskDescription description;
    description.setNumTasks(numTasks);
    description.setLength(a);
    // TODO: memory and disk
    if (currentTasks.empty()) {
        // Get a release date so that the first task is still executing
        currentrfirst = -a / currentPower;
        currentrfirst = tsum = r = getRandomReleaseDelta();
        alreadyExecuted -= Duration(r);
    } else {
        r = getRandomReleaseDelta();
        endtime = currentTasks.back()->getDescription().getDeadline();
    }
    Time creationTime = now + Duration(r);
    tsum += a * numTasks / currentPower;
    if (endtime < now + Duration(tsum))
        endtime = now + Duration(tsum);
    description.setDeadline(endtime + Duration(getRandomAppLength() / numTasks / currentPower));
    for (unsigned int taskid = 0; taskid < numTasks; ++taskid) {
        boost::shared_ptr<TestTask> newTask(new TestTask(CommAddress(), appId, taskid, description, currentPower));
        newTask->setCreationTime(creationTime);
        currentTasks.push_back(newTask);
    }
    ++appId;
    boost::static_pointer_cast<TestTask>(currentTasks.front())->execute(alreadyExecuted);
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


RandomQueueGenerator::RandomQueueGenerator() {
    seed(std::time(NULL));
}

} // namespace stars
