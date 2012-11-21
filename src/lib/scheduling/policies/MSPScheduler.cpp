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

#include "Logger.hpp"
#include "MSPScheduler.hpp"
#include <algorithm>
#include <cmath>
using std::vector;
using std::list;


double MSPScheduler::sortMinSlowness(list<TaskProxy> & proxys, const vector<double> & lBounds, list<boost::shared_ptr<Task> > & tasks) {
    if (!proxys.empty()) {
        TaskProxy::sortMinSlowness(proxys, lBounds);

        // Reconstruct task list and calculate minimum slowness
        tasks.clear();
        double minSlowness = 0.0;
        Time e = Time::getCurrentTime();
        // For each task, calculate finishing time
        for (list<TaskProxy>::iterator i = proxys.begin(); i != proxys.end(); ++i) {
            tasks.push_back(i->origin);
            e += Duration(i->t);
            double slowness = (e - i->rabs).seconds() / i->a;
            if (slowness > minSlowness)
                minSlowness = slowness;
        }

        return minSlowness;
    } else return 0.0;
}


void MSPScheduler::reschedule() {
    double minSlowness = 0.0;
    vector<double> svCur(switchValues.size());
    for (size_t i = 0; i < svCur.size(); ++i) svCur[i] = switchValues[i].first;

    if (!proxys.empty()) {
        // Adjust the time of the first task
        proxys.front().t = proxys.front().origin->getEstimatedDuration().seconds();
        minSlowness = sortMinSlowness(proxys, svCur, tasks);
    }

    LogMsg("Ex.Sch.MS", DEBUG) << "Current minimum slowness: " << minSlowness;

    info.setAvailability(backend.impl->getAvailableMemory(), backend.impl->getAvailableDisk(), proxys, svCur, backend.impl->getAveragePower(), minSlowness);

    // Start first task if it is not executing yet.
    if (!tasks.empty()) {
        if (tasks.front()->getStatus() == Task::Prepared) tasks.front()->run();
        // Program a timer
        rescheduleAt(Time::getCurrentTime() + Duration(600.0));
    }
}


unsigned int MSPScheduler::acceptable(const TaskBagMsg & msg) {
    // Always accept new tasks
    unsigned int numAccepted = msg.getLastTask() - msg.getFirstTask() + 1;
    LogMsg("Ex.Sch.MS", INFO) << "Accepting " << numAccepted << " tasks from " << msg.getRequester();
    return numAccepted;
}


void MSPScheduler::removeTask(const boost::shared_ptr<Task> & task) {
    // Look for the proxy
    list<TaskProxy>::iterator p = proxys.begin();
    while (p != proxys.end() && p->id != task->getTaskId()) ++p;
    if (p != proxys.end()) {
        vector<double> svOld;
        // Calculate bounds with the rest of the tasks, except the first
        for (list<TaskProxy>::iterator it = ++proxys.begin(); it != proxys.end(); ++it)
            if (it->a != p->a) {
                double l = (p->rabs - it->rabs).seconds() / (it->a - p->a);
                if (l > 0.0) {
                    svOld.push_back(l);
                }
            }
        std::sort(svOld.begin(), svOld.end());
        // Create a new list of switch values without the removed ones, counting occurrences
        vector<std::pair<double, int> > svCur(switchValues.size());
        vector<std::pair<double, int> >::iterator in1 = switchValues.begin(), out = svCur.begin();
        vector<double>::iterator in2 = svOld.begin();
        while (in1 != switchValues.end() && in2 != svOld.end()) {
            if (in1->first != *in2) *out++ = *in1++;
            else {
                if (in1->second > 1) {
                    out->first = in1->first;
                    (out++)->second = in1->second - 1;
                }
                ++in1;
                ++in2;
            }
        }
        if (in2 == svOld.end())
            out = std::copy(in1, switchValues.end(), out);
        if (out != svCur.end())
            svCur.erase(out, svCur.end());
        switchValues.swap(svCur);
        // Remove the proxy
        proxys.erase(p);
    }
}


void MSPScheduler::acceptTask(const boost::shared_ptr<Task> & task) {
    // Add a new proxy for this task
    proxys.push_back(TaskProxy(task));
    vector<double> svNew;
    // Calculate bounds with the rest of the tasks, except the first
    for (list<TaskProxy>::iterator it = ++proxys.begin(); it != proxys.end(); ++it)
        if (it->a != proxys.back().a) {
            double l = (proxys.back().rabs - it->rabs).seconds() / (it->a - proxys.back().a);
            if (l > 0.0) {
                svNew.push_back(l);
            }
        }
    std::sort(svNew.begin(), svNew.end());
    // Create new vector of values, counting occurrences
    vector<std::pair<double, int> > svCur(switchValues.size() + svNew.size());
    vector<std::pair<double, int> >::iterator in1 = switchValues.begin(), out = svCur.begin();
    vector<double>::iterator in2 = svNew.begin();
    while (in1 != switchValues.end() && in2 != svNew.end()) {
        if (in1->first < *in2) *out++ = *in1++;
        else if (in1->first > *in2) *out++ = std::make_pair(*in2++, 1);
        else {
            out->first = in1->first;
            (out++)->second = (in1++)->second + 1;
            ++in2;
        }
    }
    if (in1 == switchValues.end())
        while (in2 != svNew.end()) *out++ = std::make_pair(*in2++, 1);
    if (in2 == svNew.end())
        out = std::copy(in1, switchValues.end(), out);
    if (out != svCur.end())
        svCur.erase(out, svCur.end());
    switchValues.swap(svCur);
}
