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

#ifndef PERFECTSCHEDULER_H_
#define PERFECTSCHEDULER_H_

#include <vector>
#include <map>
#include <list>
#include <string>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Simulator.hpp"
#include "TaskBagMsg.hpp"
#include "Simulator.hpp"
#include "Distributions.hpp"

class PerfectScheduler : public Simulator::InterEventHandler {
public:
    struct TaskDesc {
        boost::shared_ptr<TaskBagMsg> msg;
        unsigned int tid;
        Time d, r;
        Duration a;
        bool running;
        bool operator<(const TaskDesc & rt) const {
            return running || (!rt.running && d <= rt.d);
        }
        TaskDesc(boost::shared_ptr<TaskBagMsg> m) : msg(m), r(Time::getCurrentTime()), running(false) {}
    };

    ~PerfectScheduler();
    bool blockEvent(const Simulator::Event & ev);
    bool blockMessage(uint32_t src, uint32_t dst, const boost::shared_ptr<BasicMsg> & msg);
    void afterEvent(const Simulator::Event & ev);

    const std::list<TaskDesc> & getQueue(int n) const {
        return queues[n];
    }

    static boost::shared_ptr<PerfectScheduler> createScheduler(const std::string & type);

protected:
    std::vector<std::list<TaskDesc> > queues;
    fs::ofstream os;
    std::vector<Time> queueEnds;
    Time maxQueue;

    Time current;
    unsigned long int inTraffic, outTraffic;

    virtual void newApp(boost::shared_ptr<TaskBagMsg> msg) = 0;
    virtual void taskFinished(unsigned int node);
    void sendOneTask(unsigned int to);
    void acceptTasks(unsigned int node, uint32_t rid, uint32_t tid, unsigned int num = 1);
    void addToQueue(const TaskDesc & task, unsigned int node);

    PerfectScheduler();
};


#endif /*PERFECTSCHEDULER_H_*/