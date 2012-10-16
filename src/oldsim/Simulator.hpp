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

#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include <queue>
#include <map>
#include <vector>
#include <list>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <log4cpp/Layout.hh>
#include "BasicMsg.hpp"
#include "SimulationCase.hpp"
#include "StarsNode.hpp"
#include "Properties.hpp"
#include "Time.hpp"
#include "LibStarsStatistics.hpp"
#include "PerformanceStatistics.hpp"
#include "TrafficStatistics.hpp"
#include "FailureGenerator.hpp"
namespace pt = boost::posix_time;
namespace fs = boost::filesystem;
class PerfectScheduler;


struct AddrIO {
    uint32_t addr;
    AddrIO(uint32_t a) : addr(a) {}
    friend std::ostream & operator<<(std::ostream & os, const AddrIO & t) {
        uint32_t val1, val2, val3, val4 = t.addr;
        std::ostringstream oss;
        val1 = val4 & 255;
        val4 >>= 8;
        val2 = val4 & 255;
        val4 >>= 8;
        val3 = val4 & 255;
        val4 >>= 8;
        oss << val4 << "." << val3 << "." << val2 << "." << val1;
        return os << oss.str();
    }
};


class Simulator {
public:
    struct Event {
        int id;
        static int lastEventId;
        // Time in microseconds
        Time creationTime;
        Time txTime;
        Duration txDuration;
        Time t;
        boost::shared_ptr<BasicMsg> msg;
        uint32_t from, to;
        bool active;
        bool inRecvQueue;
        unsigned int size;
        Event(Time c, boost::shared_ptr<BasicMsg> initmsg, unsigned int sz) : id(lastEventId++), creationTime(c),
            txTime(c), txDuration(0.0), t(c), msg(initmsg), active(true), inRecvQueue(false), size(sz) {}
        Event(Time c, Time outQueue, Duration tx, Duration d, boost::shared_ptr<BasicMsg> initmsg,
            unsigned int sz) : id(lastEventId++), creationTime(c), txTime(outQueue), txDuration(tx), t(txTime + tx + d),
            msg(initmsg), active(true), inRecvQueue(false), size(sz) {}
        Event(Time c, Duration d, boost::shared_ptr<BasicMsg> initmsg, unsigned int sz) : id(lastEventId++),
            creationTime(c), txTime(c), t(c + d),
            msg(initmsg), active(true), inRecvQueue(false), size(sz) {}
    };

    struct NodeNetInterface {
        // In and Out network interface model
        Time inQueueFreeTime, outQueueFreeTime;
        double inBW, outBW;
    };

    static Simulator & getInstance() {
        static Simulator instance;
        return instance;
    }

    void setProperties(Properties & property);
    void run();
    void stepForward();
    void stop() { doStop = true; }
    bool isPrepared() const { return !doStop; }
    void showStatistics();
    void finish();
    void showInformation();

    bool inEvent() const { return p != NULL; }
    void setCurrentNode(uint32_t n) { currentNode = &routingTable[n]; }
    unsigned long int getNumNodes() const { return routingTable.size(); }
    StarsNode & getNode(unsigned int i) { return routingTable[i]; }
    const NodeNetInterface & getNetInterface(unsigned int i) const { return iface[i]; }
    StarsNode & getCurrentNode() { return *currentNode; }
    int getCurrentEventId() const { return p != NULL ? p->id : 0; }
    Event & getCurrentEvent() { return *p; }
    bool emptyEventQueue() const { return events.size() == inactiveEvents; }
    const std::list<Event *> & getGeneratedEvents() const { return generatedEvents; }
    const fs::path & getResultDir() const { return resultDir; }
    PerformanceStatistics & getPerfStats() { return pstats; }
    LibStarsStatistics & getStarsStatistics() { return sstats; }
    const boost::shared_ptr<PerfectScheduler> & getPerfectScheduler() { return ps; }
    const boost::shared_ptr<SimulationCase> & getSimulationCase() { return simCase; }

    // Network methods
    unsigned int sendMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg);
    unsigned int injectMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg,
            Duration d = Duration(0.0), bool withOpDuration = false);

    // Time methods
    Time getCurrentTime() const { return time; }
    pt::time_duration getRealTime() const { return real_time + (pt::microsec_clock::local_time() - start); }
    int setTimer(uint32_t dst, Time when, boost::shared_ptr<BasicMsg> msg);
    void cancelTimer(int timerId);

    void progressLog(const std::string & msg);
    bool isLogEnabled(const std::string & category, int priority);
    boost::iostreams::filtering_ostream & getDebugArchive() { return debugArchive; }

    static unsigned long int getMsgSize(boost::shared_ptr<BasicMsg> msg);

    // Return a random double in interval (0, 1]
    static double uniform01() {
        double r = (std::rand() + 1.0) / (RAND_MAX + 1.0);
        return r;
    }

    // Return a random double in interval (min, max]
    static double uniform(double min, double max) {
        double r = min + (max - min) * uniform01();
        return r;
    }

    static double exponential(double mean) {
        double r = - std::log(uniform01()) * mean;
        return r;
    }

    static double pareto(double xm, double k, double max) {
        // xm > 0 and k > 0
        double r;
        do
            r = xm / std::pow(uniform01(), 1.0 / k);
        while (r > max);
        return r;
    }

    static double normal(double mu, double sigma) {
        const double pi = 3.14159265358979323846;   //approximate value of pi
        double r = mu + sigma * std::sqrt(-2.0 * std::log(uniform01())) * std::cos(2.0 * pi * uniform01());
        return r;
    }

    static int discretePareto(int min, int max, int step, double k) {
        int r = min + step * ((int)std::floor(pareto(step, k, max - min) / step) - 1);
        return r;
    }

    // Return a random int in interval [min, max] with step
    static int uniform(int min, int max, int step = 1) {
        int r = min + step * ((int)std::ceil(std::floor((double)(max - min) / (double)step + 1.0) * uniform01()) - 1);
        return r;
    }

private:
    class ptrCompare {
    public:
        bool operator()(const Event * l, const Event * r) {
            return l->t > r->t || (l->t == r->t && l->id > r->id);
        }
    };

    // Simulation framework
    boost::shared_ptr<SimulationCase> simCase;
    std::vector<StarsNode> routingTable;
    std::vector<NodeNetInterface> iface;
    Time time;
    std::priority_queue<Event *, std::vector<Event *>, ptrCompare > events;
    std::map<int, Event *> timers;

    Event * p;
    StarsNode * currentNode;
    std::list<Event *> generatedEvents;
    unsigned int inactiveEvents;
    double minDelay;   // network min delay
    double maxDelay;   // network max delay
    fs::path resultDir;
    fs::ofstream progressFile, debugFile;
    boost::iostreams::filtering_ostream debugArchive;
    boost::shared_ptr<PerfectScheduler> ps;
    FailureGenerator fg;

    // Simulation global statistics
    PerformanceStatistics pstats;
    LibStarsStatistics sstats;
    TrafficStatistics tstats;
    pt::ptime start, end, opStart;
    pt::time_duration real_time;
    unsigned long int numEvents;
    unsigned long int totalBytesSent;
    unsigned long int numMsgSent;
    bool measureSize;
    unsigned long int maxEvents;
    pt::time_duration maxRealTime;
    Duration maxSimTime;
    unsigned int maxMemUsage;
    unsigned int showStep;
    bool doStop;

    // Singleton
    Simulator();
};

#endif /*SIMULATOR_H_*/
