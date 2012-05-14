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

#include "Logger.hpp"
#include "MinStretchScheduler.hpp"
#include <algorithm>
#include <cmath>
using std::list;


double MinStretchScheduler::sortMinStretch(const list<boost::shared_ptr<Task> > & tasks, list<StretchInformation::AppDesc> & apps) {
    // Create an application list and group tasks from same request
    // Trivial case
    if (tasks.empty()) {
        return 0.0;
    }

    // Group tasks into applications
    Time now = Time::getCurrentTime();
    // First (running) task is counted as a a single app on its own
    StretchInformation::AppDesc firstApp(tasks.begin(), ++tasks.begin(), now);
    if (tasks.size() == 1) {
        apps.push_back(firstApp);
        return (firstApp.a - firstApp.r) / firstApp.w;
    }

    StretchInformation::AppDesc::taskIterator start = ++tasks.begin();
    for (StretchInformation::AppDesc::taskIterator t = start; t != tasks.end(); ++t) {
        if ((*t)->getClientRequestId() != (*start)->getClientRequestId()) {
            // A new app starts
            apps.push_back(StretchInformation::AppDesc(start, t, now));
            start = t;
        }
    }
    apps.push_back(StretchInformation::AppDesc(start, tasks.end(), now));

    // Calculate cross stretch values
    std::vector<double> boundaries(1, 0.0);
    for (list<StretchInformation::AppDesc>::iterator i = apps.begin(); i != apps.end(); ++i)
        for (list<StretchInformation::AppDesc>::iterator j = i; j != apps.end(); ++j)
            if (i->w != j->w) {
                double s = (j->r - i->r) / (i->w - j->w);
                if (s > 0.0)
                    boundaries.push_back(s);
            }
    std::sort(boundaries.begin(), boundaries.end());

    boundaries.push_back(boundaries.back() + 1.0);

    // Calculate interval by binary search
    unsigned int minSi = 0;
    unsigned int maxSi = boundaries.size() - 2;
    while (maxSi != minSi) {
        unsigned int medSi = floor((minSi + maxSi) / 2);
        double medStretch = (boundaries[medSi] + boundaries[medSi + 1]) / 2.0;
        // Check stretch
        for (list<StretchInformation::AppDesc>::iterator i = apps.begin(); i != apps.end(); ++i)
            i->setStretch(medStretch);
        apps.sort();
        // For each app, check whether it is going to finish in time or not.
        // First app first
        firstApp.setStretch(medStretch);
        if (firstApp.d >= firstApp.a) {
            double e = firstApp.a;
            for (list<StretchInformation::AppDesc>::iterator i = apps.begin(); i != apps.end(); ++i) {
                if ((i->d - e) < i->a) {
                    // Not finishing on time, try with a bigger stretch
                    minSi = medSi + 1;
                    break;
                }
                e += i->a;
            }
            if (minSi <= medSi) maxSi = medSi;
        } else {
            minSi = medSi + 1;
        }
    }
    // Sort them one last time
    double medStretch = (boundaries[maxSi] + boundaries[maxSi + 1]) / 2.0;
    // Check stretch
    for (list<StretchInformation::AppDesc>::iterator i = apps.begin(); i != apps.end(); ++i)
        i->setStretch(medStretch);
    apps.sort();

    apps.push_front(firstApp);
    double min = 0.0, e = 0.0;
    // For each task, calculate finishing time
    for (list<StretchInformation::AppDesc>::iterator i = apps.begin(); i != apps.end(); ++i) {
        e += i->a;
        double stretch = (e - i->r) / i->w;
        if (stretch > min)
            min = stretch;
    }
    return min;
}


void MinStretchScheduler::reschedule() {
    list<StretchInformation::AppDesc> apps;
    double minStretch = sortMinStretch(tasks, apps);
    LogMsg("Ex.Sch.MS", DEBUG) << "Current minimum stretch: " << minStretch;

    // Sort tasks by apps
    list<boost::shared_ptr<Task> > newTasks;
    for (list<StretchInformation::AppDesc>::iterator i = apps.begin(); i != apps.end(); ++i)
        newTasks.insert(newTasks.end(), i->firstTask, i->endTask);
    tasks.swap(newTasks);

    info.setAvailability(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(), apps, backend.impl->getAveragePower());

    // Start first task if it is not executing yet.
    if (!tasks.empty()) {
        if (tasks.front()->getStatus() == Task::Prepared) tasks.front()->run();
        // Program a timer
        rescheduleAt(Time::getCurrentTime() + Duration(600.0));
    }
}


unsigned int MinStretchScheduler::accept(const TaskBagMsg & msg) {
    // Always accept new tasks
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    LogMsg("Ex.Sch.MS", INFO) << "Accepting " << numAccepted << " tasks from " << msg.getRequester();

    // Now create the tasks and add them to the task list
    for (unsigned int i = 0; i < numAccepted; i++)
        tasks.push_back(backend.impl->createTask(msg.getRequester(), msg.getRequestId(), msg.getFirstTask() + i, msg.getMinRequirements()));
    reschedule();
    notifySchedule();
    return numAccepted;
}
