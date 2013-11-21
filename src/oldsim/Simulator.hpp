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
#include "Variables.hpp"
namespace pt = boost::posix_time;
namespace fs = boost::filesystem;
class CentralizedScheduler;


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
        std::shared_ptr<BasicMsg> msg;
        uint32_t from, to;
        bool inRecvQueue;
        unsigned int size;
        Event(Time c, std::shared_ptr<BasicMsg> initmsg, unsigned int sz) : id(lastEventId++), creationTime(c),
            txTime(c), txDuration(0.0), t(c), msg(initmsg), inRecvQueue(false), size(sz) {}
        Event(Time c, Time outQueue, Duration tx, Duration d, std::shared_ptr<BasicMsg> initmsg,
            unsigned int sz) : id(lastEventId++), creationTime(c), txTime(outQueue), txDuration(tx), t(txTime + tx + d),
            msg(initmsg), inRecvQueue(false), size(sz) {}
        Event(Time c, Duration d, std::shared_ptr<BasicMsg> initmsg, unsigned int sz) : id(lastEventId++),
            creationTime(c), txTime(c), t(c + d),
            msg(initmsg), inRecvQueue(false), size(sz) {}
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
    uint32_t getCurrentNodeNum() { return currentNode - &routingTable[0]; }
    int getCurrentEventId() const { return p != NULL ? p->id : 0; }
    Event & getCurrentEvent() { return *p; }
    bool emptyEventQueue() const { return events.empty(); }
    const std::list<Event *> & getGeneratedEvents() const { return generatedEvents; }
    const fs::path & getResultDir() const { return resultDir; }
    PerformanceStatistics & getPerfStats() { return pstats; }
    LibStarsStatistics & getStarsStatistics() { return sstats; }
    const std::shared_ptr<CentralizedScheduler> & getCentralizedScheduler() { return cs; }
    const std::shared_ptr<SimulationCase> & getSimulationCase() { return simCase; }

    // Network methods
    unsigned int sendMessage(uint32_t src, uint32_t dst, std::shared_ptr<BasicMsg> msg);
    unsigned int injectMessage(uint32_t src, uint32_t dst, std::shared_ptr<BasicMsg> msg,
            Duration d = Duration(0.0), bool withOpDuration = false);

    // Time methods
    Time getCurrentTime() const { return time; }
    pt::time_duration getRealTime() const { return real_time + (pt::microsec_clock::local_time() - start); }

    bool isLogEnabled(const std::string & category, int priority);
    boost::iostreams::filtering_ostream & getDebugStream() { return debugStream; }
    boost::iostreams::filtering_ostream & getProgressStream() { return progressStream; }
    bool isLastLogMoment();

    static unsigned long int getMsgSize(std::shared_ptr<BasicMsg> msg);

private:
    class ptrCompare {
    public:
        bool operator()(const Event * l, const Event * r) {
            return l->t > r->t || (l->t == r->t && l->id > r->id);
        }
    };

    // Simulation framework
    std::shared_ptr<SimulationCase> simCase;
    std::vector<StarsNode> routingTable;
    std::vector<NodeNetInterface> iface;
    Time time;
    std::priority_queue<Event *, std::vector<Event *>, ptrCompare > events;

    Event * p;
    StarsNode * currentNode;
    std::list<Event *> generatedEvents;
    ParetoVariable netDelay; // network delay
    fs::path resultDir;
    fs::ofstream progressFile, debugFile;
    boost::iostreams::filtering_ostream debugStream;
    boost::iostreams::filtering_ostream progressStream;
    StarsNode * debugNode;
    Time lastDebugTime;
    StarsNode * lastDebugNode;
    std::shared_ptr<CentralizedScheduler> cs;
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
    pt::time_duration maxRealTime;
    Duration maxSimTime;
    unsigned int maxMemUsage;
    unsigned int showStep;
    bool doStop;

    // Singleton
    Simulator();
};

#endif /*SIMULATOR_H_*/
