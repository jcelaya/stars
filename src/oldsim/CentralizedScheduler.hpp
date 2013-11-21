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

#ifndef CENTRALIZEDSCHEDULER_H_
#define CENTRALIZEDSCHEDULER_H_

#include <vector>
#include <map>
#include <list>
#include <string>
#include <utility>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Time.hpp"
#include "Simulator.hpp"
#include "RescheduleMsg.hpp"
class TaskBagMsg;

class CentralizedScheduler {
public:
    struct TaskDesc {
        std::shared_ptr<TaskBagMsg> msg;
        unsigned int tid;
        Time d, r;
        Duration a;
        bool running;
        bool operator<(const TaskDesc & rt) const {
            return d < rt.d || (
                    d == rt.d && (msg->getRequestId() < rt.msg->getRequestId() || (
                            msg->getRequestId() == rt.msg->getRequestId() && tid <= rt.tid))); }
        TaskDesc(std::shared_ptr<TaskBagMsg> m) : msg(m), tid(0), r(Time::getCurrentTime()), running(false) {}
        friend std::ostream & operator<<(std::ostream & os, const TaskDesc & t) {
            return os << '(' << (t.running ? 'r' : 'p') << ' ' << t.r.getRawDate() << '-' << t.a.seconds() << '-' << t.d.getRawDate() << ')';
        }
    };

    virtual ~CentralizedScheduler() {}
    virtual bool blockEvent(const Simulator::Event & ev);
    virtual bool blockMessage(const std::shared_ptr<BasicMsg> & msg);

    const std::list<TaskDesc> & getQueue(int n) const { return queues[n]; }

    void showStatistics();

    static std::shared_ptr<CentralizedScheduler> createScheduler(const std::string & type);

protected:
    Simulator & sim;
    std::vector<std::list<TaskDesc> > queues;
    unsigned long int inTraffic, outTraffic;
    uint64_t rescheduleSequence;

    virtual void newApp(std::shared_ptr<TaskBagMsg> msg) = 0;
    virtual void taskFinished(unsigned int node);
    void sortQueue(unsigned int node);

    CentralizedScheduler();

    std::shared_ptr<stars::RescheduleMsg> getRescheduleMsg(std::list<TaskDesc> & queue);
};


#endif /* CENTRALIZEDSCHEDULER_H_ */
