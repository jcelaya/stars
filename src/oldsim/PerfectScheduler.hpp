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

#ifndef PERFECTSCHEDULER_H_
#define PERFECTSCHEDULER_H_

#include <vector>
#include <list>
#include <string>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Time.hpp"
#include "Simulator.hpp"
class TaskBagMsg;

class PerfectScheduler {
public:
    struct TaskDesc {
        boost::shared_ptr<TaskBagMsg> msg;
        unsigned int tid;
        Time d, r;
        Duration a;
        bool running;
        bool operator<(const TaskDesc & rt) const { return running || (!rt.running && d <= rt.d); }
        TaskDesc(boost::shared_ptr<TaskBagMsg> m) : msg(m), tid(0), r(Time::getCurrentTime()), running(false) {}
    };

    virtual ~PerfectScheduler();
    bool blockEvent(const Simulator::Event & ev);
    bool blockMessage(const boost::shared_ptr<BasicMsg> & msg);

    const std::list<TaskDesc> & getQueue(int n) const { return queues[n]; }

    static boost::shared_ptr<PerfectScheduler> createScheduler(const std::string & type);

protected:
    Simulator & sim;
    std::vector<std::list<TaskDesc> > queues;
    fs::ofstream os;
    Time maxQueue;
    std::vector<Time> queueEnds;

    Time current;
    unsigned long int inTraffic, outTraffic;

    virtual void newApp(boost::shared_ptr<TaskBagMsg> msg) = 0;
    virtual void taskFinished(unsigned int node);
    void sendOneTask(unsigned int to);
    void addToQueue(const TaskDesc & task, unsigned int node);

    PerfectScheduler();
};


#endif /*PERFECTSCHEDULER_H_*/
