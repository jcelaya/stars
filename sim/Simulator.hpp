/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
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
#include <log4cpp/Layout.hh>
#include "portable_binary_oarchive.hpp"
#include "BasicMsg.hpp"
#include "PeerCompNode.hpp"
#include "Properties.hpp"
#include "Time.hpp"
#include "PeerCompStatistics.hpp"
#include "PerformanceStatistics.hpp"
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

	class InterEventHandler {
	protected:
		Simulator & sim;
	public:
		InterEventHandler() : sim(Simulator::getInstance()) {}
		virtual ~InterEventHandler() {}
		virtual bool blockEvent(const Event & ev) { return false; }
		virtual bool blockMessage(uint32_t src, uint32_t dst, const boost::shared_ptr<BasicMsg> & msg) { return false; }
		virtual void beforeEvent(const Event & ev) {}
		virtual void afterEvent(const Event & ev) {}
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

	~Simulator();

	void registerHandler(const boost::shared_ptr<InterEventHandler> & handler) {
		interEventHandlers.push_back(handler);
	}

	void setProperties(Properties & property);
	void run();
	void stepForward();
	void stop() { doStop = true; }
	bool isPrepared() const { return !doStop; }
	void showStatistics();

	bool inEvent() const { return p != NULL; }
	void setCurrentNode(uint32_t n) { currentNode = &routingTable[n]; }
	unsigned long int getNumNodes() const { return routingTable.size(); }
	PeerCompNode & getNode(unsigned int i) { return routingTable[i]; }
	const NodeNetInterface & getNetInterface(unsigned int i) const { return iface[i]; }
	PeerCompNode & getCurrentNode() { return *currentNode; }
	int getCurrentEventId() const { return p != NULL ? p->id : 0; }
	bool emptyEventQueue() const { return events.size() == inactiveEvents; }
	const std::list<Event *> & getGeneratedEvents() const { return generatedEvents; }
	const fs::path & getResultDir() const { return resultDir; }
	PerformanceStatistics & getPStats() { return pstats; }
	PeerCompStatistics & getPCStats() { return pcstats; }
	const boost::shared_ptr<PerfectScheduler> & getPerfectScheduler() { return ps; }

	// Network methods
	unsigned int sendMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg);
	unsigned int injectMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg,
			Duration d = Duration(0.0), bool withOpDuration = false);

	// Time methods
	Time getCurrentTime() const { return time; }
	pt::time_duration getRealTime() const { return real_time + (pt::microsec_clock::local_time() - start); }
	int setTimer(uint32_t dst, Time when, boost::shared_ptr<BasicMsg> msg);
	void cancelTimer(int timerId);

	void progressLog(const std::list<LogMsg::AbstractTypeContainer *> & values);
	void log(tracked_string * category, int priority, const std::list<LogMsg::AbstractTypeContainer *> & values);

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
	std::vector<PeerCompNode> routingTable;
	std::vector<NodeNetInterface> iface;
	Time time;
	std::priority_queue<Event *, std::vector<Event *>, ptrCompare > events;
	std::map<int, Event *> timers;
	std::list<boost::shared_ptr<InterEventHandler> > interEventHandlers;

	Event * p;
	PeerCompNode * currentNode;
	std::list<Event *> generatedEvents;
	unsigned int inactiveEvents;
	double minDelay;   // network min delay
	double maxDelay;   // network max delay
	fs::path resultDir;
	fs::ofstream progressFile, debugFile;
	boost::scoped_ptr<portable_binary_oarchive> debugArchive;
	PerformanceStatistics pstats;
	boost::shared_ptr<PerfectScheduler> ps;

	// Simulation global statistics
	PeerCompStatistics pcstats;
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
