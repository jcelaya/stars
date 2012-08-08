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

#include <vector>
#include <list>
#include <sqlite3.h>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "Logger.hpp"
#include "../SimulationCase.hpp"
#include "../Simulator.hpp"
#include "Time.hpp"
#include "DispatchCommandMsg.hpp"
#include "TaskBagAppDatabase.hpp"
#include "../SimTask.hpp"
#include "RequestGenerator.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "RequestTimeout.hpp"
#include "TimeConstraintInfo.hpp"
#include "AppFinishedMsg.hpp"
#include "../Distributions.hpp"
using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::gregorian;


int saveDb(sqlite3 *pInMemory, const char *zFilename){
    int rc;                   /* Function return code */
    sqlite3 *pFile;           /* Database connection opened on zFilename */
    sqlite3_backup *pBackup;  /* Backup object used to copy data */

    /* Open the database file identified by zFilename. Exit early if this fails
     * for any reason. */
    rc = sqlite3_open(zFilename, &pFile);
    if (rc == SQLITE_OK) {
        /* Set up the backup procedure to copy from the "main" database of
        ** connection pFile to the main database of connection pInMemory.
        ** If something goes wrong, pBackup will be set to NULL and an error
        ** code and  message left in connection pTo.
        **
        ** If the backup object is successfully created, call backup_step()
        ** to copy data from pFile to pInMemory. Then call backup_finish()
        ** to release resources associated with the pBackup object.  If an
        ** error occurred, then  an error code and message will be left in
        ** connection pTo. If no error occurred, then the error code belonging
        ** to pTo is set to SQLITE_OK.
        */
        pBackup = sqlite3_backup_init(pFile, "main", pInMemory, "main");
        if( pBackup ){
            (void)sqlite3_backup_step(pBackup, -1);
            (void)sqlite3_backup_finish(pBackup);
        }
        rc = sqlite3_errcode(pFile);
    }

    /* Close the database connection opened on database file zFilename
    ** and return the result of this function. */
    (void)sqlite3_close(pFile);
    return rc;
}


//class constantLoad : public SimulationCase {
//	unsigned int numSearches, nextSearch;
//	unsigned int numSimult, waiting;
//	long timeToNext;
//	RequestGenerator rg;
//
//public:
//	constantLoad(const Properties & p) : SimulationCase(getName, p), rg(p) {
//		// Prepare the properties
//	}
//
//	static const string getName() { return string("constantLoad"); }
//
//	void preStart() {
//		// Before running simulation
//		sim.registerHandler(*this);
//
//		// Simulation limit
//		numSearches = property("num_searches", 1);
//		testDisCat().warnStream() << "Performing " << numSearches << " searches.";
//
//		// Global variables
//		numSimult = ceil(sim.getNumNodes() / as);
//
//		double systemLoad, totalPower;
//		totalPower = 0.0;
//		for (unsigned int i = 0; i < sim.getNumNodes(); i++)
//			totalPower += sim.getNode(i).getAveragePower();
//		systemLoad = property("system_load", 1.0);
//		testDisCat().debugStream() << "Total power of " << totalPower << " and system load of " << systemLoad;
//
//		// Prepare the search requests
//		shared_ptr<DispatchCommandMsg> dcm = rg.generate(sim.getNode(0));
//
//		// Calculate next send
//		timeToNext = (dcm->getMinRequirements()->getAppLength() * numSimult / totalPower) / systemLoad * 1000;
//
//		// Send first requests
//		for (nextSearch = 1; nextSearch <= numSimult; nextSearch++) {
//			shared_ptr<DispatchCommandMsg> dcmTmp = dcm->clone();
//			shared_ptr<TaskDescription> minReq = dcmTmp->getMinRequirements();
//			uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
//			// Calculate correct deadline
//			double appPrio = Simulator::normal(muPrio, sigmaPrio);
//			double d = minReq->getAppLength() / (sim.getNode(client).getAveragePower() * appPrio);
//			if (appPrio <= 0 || d > 31536000) d = 31536000;
//			minReq->setDeadline(Simulator::firstRef + seconds(d));
//			// Send this message to the client
//			sim.injectMessage(client, client, dcmTmp, 0);
//			jstat->createRequest(dcmTmp->getRequestId(), client, 0, dcmTmp->getNumTasks(), minReq->getAppLength(),
//					(minReq->getLength() / sim.getNode(client).getAveragePower()) * 1000,
//					d * 1000);
//			testDisCat().debugStream() << "Starting search " << dcm.getRequestId() << " from " << CommAddress(client) << " for " << dcm.getNumTasks()
//				<< " nodes at " << Simulator::firstRef << " until " << minReq->getDeadline();
//		}
//		waiting = numSimult;
//	}
//
//	void afterEvent(const Simulator::Event & ev) {
//		if (typeid(*ev.msg) == typeid(DispatchCommandMsg) && nextSearch <= numSearches && !(--waiting)) {
//			shared_ptr<TaskDescription> minReq = dcm.getMinRequirements();
//			for (unsigned int i = 0; i < numSimult; nextSearch++, i++) {
//				uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
//				dcm.setRequestId(nextSearch);
//				double appPrio = Simulator::normal(muPrio, sigmaPrio);
//				double d = minReq->getAppLength() / (sim.getNode(client).getAveragePower() * appPrio);
//				if (appPrio <= 0 || d > 31536000) d = 31536000;
//				minReq->setDeadline(Simulator::firstRef + milliseconds(sim.getCurrentTime() + timeToNext) + seconds(d));
//				// Send this message to the client
//				sim.injectMessage(client, client, dcm.clone(), timeToNext);
//				jstat->createRequest(dcm.getRequestId(), client, sim.getCurrentTime() + timeToNext,
//						dcm.getNumTasks(), minReq->getAppLength(),
//						(minReq->getLength() / sim.getNode(client).getAveragePower()) * 1000,
//						sim.getCurrentTime() + timeToNext + d * 1000);
//				testDisCat().debugStream() << "Starting search " << dcm.getRequestId() << " from " << CommAddress(client) << " for " << dcm.getNumTasks()
//					<< " nodes at " << (sim.getCurrentTime() + timeToNext) << " until " << minReq->getDeadline();
//				percent = (nextSearch * 100.0 / numSearches);
//			}
//			waiting = numSimult;
//		}
//	}
//
//	bool blockMessage(uint32_t src, uint32_t dst, const shared_ptr<BasicMsg> & msg) {
//		return nextSearch > numSearches && typeid(*msg) == typeid(RescheduleTimer);
//	}
//
//	void postEnd() {
//	}
//};
//REGISTER_SIMULATION_CASE(constantLoad);


class HeartbeatTimeout;


class poissonProcess : public SimulationCase {
    unsigned int numInstances, nextInstance, remainingApps;
    double meanTime;
    RequestGenerator rg;
    bool generateTraceOnly;

public:
    poissonProcess(const Properties & p) : SimulationCase(p), rg(p) {
        // Prepare the properties
    }

    static const string getName() { return string("poissonProcess"); }

    void preStart() {
        // Before running simulation

        // Simulation limit
        numInstances = property("num_searches", 1);
        LogMsg("Sim.Progress", 0) << "Performing " << numInstances << " searches.";

        // Global variables
        meanTime = property("mean_time", 60.0);
        generateTraceOnly = property("generate_trace_only", false);

        // Send first request
        uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
        shared_ptr<DispatchCommandMsg> dcm = rg.generate(sim.getNode(client), Time());
        // Send this message to the client
        sim.injectMessage(client, client, dcm, Duration());
        nextInstance = 1;
        remainingApps = 0;
    }

    void beforeEvent(const Simulator::Event & ev) {
        if (typeid(*ev.msg) == typeid(DispatchCommandMsg) && nextInstance < numInstances) {
            // Calculate next send
            Duration timeToNext(Simulator::exponential(meanTime));

            uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
            shared_ptr<DispatchCommandMsg> dcm = rg.generate(sim.getNode(client), sim.getCurrentTime() + timeToNext);
            // Send this message to the client
            sim.injectMessage(client, client, dcm, timeToNext);
            nextInstance++;
        }
        else if (typeid(*ev.msg) == typeid(AppFinishedMsg)) {
            percent = (remainingApps++) * 100.0 / numInstances;
        }
    }

    bool blockEvent(const Simulator::Event & ev) {
        if (generateTraceOnly && typeid(*ev.msg) == typeid(TaskBagMsg))
            return true;
        return false;
    }

    bool doContinue() const {
        return remainingApps < numInstances;
    }

    void postEnd() {
//		fs::path statDir(property("results_dir", std::string("./results")));
//		if (property("memory_db", true))
//			saveDb(TaskBagAppDatabase::getInstance().getDatabase().getDatabase(), (statDir / fs::path("stars.db")).c_str());
    }
};
REGISTER_SIMULATION_CASE(poissonProcess);


class repeat : public SimulationCase {
    struct AppInstance {
        unsigned int appNum;
        uint32_t client;
        Time release;
        shared_ptr<DispatchCommandMsg> dcm;
        bool operator<(const AppInstance & r) const { return appNum < r.appNum; }
    };

    unsigned int numSearches, finishedApps;
    list<AppInstance> apps;

public:
    repeat(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const string getName() { return string("repeat"); }

    void preStart() {
        // Before running simulation

        // Get apps.stat of the previous run
        fs::path appsFile(property("apps_file", string("")));
        if (!fs::exists(appsFile)) {
            LogMsg("Sim.Progress", 0) << "Apps file does not exist: " << appsFile;
            sim.stop();
            return;
        } else {
            AppInstance r;
            fs::ifstream ifs(appsFile);
            // Read and create dispatch commands
            string nextLine;
            while (ifs.good()) {
                getline(ifs, nextLine);
                // The last line is an empty line
                if (nextLine.length() == 0) break;
                // Else, jump comments
                if (nextLine[0] == '#') continue;
                // Else process this line
                r.dcm.reset(new DispatchCommandMsg);
                TaskDescription minReq;
                size_t firstpos = 0, lastpos = nextLine.find_first_of(',');
                // Get instance number
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> r.appNum;
                ostringstream appName;
                appName << "application" << r.appNum;
                r.dcm->setAppName(appName.str());
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get requester
                string req = nextLine.substr(firstpos, lastpos - firstpos);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                r.client = CommAddress(req.substr(0, req.find_first_of(':')), 0).getIPNum();
                // Get number of tasks
                unsigned int numTasks;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> numTasks;
                minReq.setNumTasks(numTasks);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get task length
                unsigned long int length;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> length;
                minReq.setLength(length);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get task memory
                unsigned int mem;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> mem;
                minReq.setMaxMemory(mem);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get task disk
                unsigned int disk;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> disk;
                minReq.setMaxDisk(disk);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get release date
                double rdate;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> rdate;
                r.release = Time(rdate * 1000000);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get deadline
                double deadline;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> deadline;
                r.dcm->setDeadline(Time(deadline * 1000000));
                // Other params
                minReq.setInputSize(property("task_input_size", 0));
                minReq.setOutputSize(property("task_output_size", 0));
                // Add request to the list
                apps.push_back(r);
                // Add an application to the database
                sim.getNode(r.client).getDatabase().createAppDescription(appName.str(), minReq);
            }
            apps.sort();
        }

        // Simulation limit
        numSearches = apps.size();
        LogMsg("Sim.Progress", 0) << "Performing " << numSearches << " searches.";

        // Send first request
        AppInstance r = apps.front();
        apps.pop_front();
        // Send this message to the client
        sim.injectMessage(r.client, r.client, r.dcm, r.release - Time());
        finishedApps = 0;
    }

    void afterEvent(const Simulator::Event & ev) {
        if (typeid(*ev.msg) == typeid(DispatchCommandMsg)) {
            // Remove the old app description
            sim.getCurrentNode().getDatabase().dropAppDescription(static_pointer_cast<DispatchCommandMsg>(ev.msg)->getAppName());
            if (!apps.empty()) {
                AppInstance r = apps.front();
                apps.pop_front();
                // Send this message to the client
                sim.injectMessage(r.client, r.client, r.dcm, r.release - sim.getCurrentTime());
            }
        }
    }

    void beforeEvent(const Simulator::Event & ev) {
        if (typeid(*ev.msg) == typeid(AppFinishedMsg)) {
            percent = (finishedApps++) * 100.0 / numSearches;
        }
    }

    bool blockMessage(uint32_t src, uint32_t dst, const shared_ptr<BasicMsg> & msg) {
        // Ignore fail tolerance
        if (msg->getName() == "HeartbeatTimeout") return true;
        else if (msg->getName() == "MonitorTimer") return true;
        return false;
    }

    bool doContinue() const {
        return finishedApps < numSearches;
    }

    void postEnd() {
        // Dump memory database to disk
//		fs::path statDir(property("results_dir", std::string("./results")));
//		if (property("memory_db", true))
//			saveDb(TaskBagAppDatabase::getInstance().getDatabase().getDatabase(), (statDir / fs::path("stars.db")).file_string().c_str());
    }
};
REGISTER_SIMULATION_CASE(repeat);


/**
 * Based on work by Shmueli and Feitelson, 2009
 */
class siteLevel : public SimulationCase {

    class UserEvent : public BasicMsg {
    public:
        MESSAGE_SUBCLASS(UserEvent);

        MSGPACK_DEFINE();
    };
    static shared_ptr<UserEvent> timer;

    struct User {
        enum {
            WAIT_JOB_FINISH,
            SLEEPING,
            WAIT_TT
        } state;
        shared_ptr<DispatchCommandMsg> lastAppCmd;
        pair<string, TaskDescription> lastApp;
        Time lastAppRT;
        long int lastInstance;
        unsigned int repeat;
        bool daytime, weekdays;
        Duration wDelta;
        static Duration morning, night;

        void setup() {
            repeat = 0;
            daytime = Simulator::uniform01() > 0.3;
            weekdays = Simulator::uniform01() > 0.2;
            wDelta = Duration(Simulator::uniform(-3600.0, 3600.0));
        }

        bool isWtime(Time now) const {
            greg_weekday today = now.to_posix_time().date().day_of_week();
            if (today == Saturday || today == Sunday) {
                if (weekdays) return false;
            } else {
                if (!weekdays) return false;
            }
            Duration nowDelta(now.to_posix_time().time_of_day().total_microseconds());
            if (nowDelta < morning + wDelta || nowDelta > night + wDelta) {
                return !daytime;
            } else {
                return daytime;
            }
        }

        Time getWakeTime(Time now) const {
            // Return the time at which this user should wake if it goes to sleep now.
            // Calculate the time for today or tomorrow
            Duration wake = (daytime ? morning : night) + wDelta;
            Duration nowDelta(now.to_posix_time().time_of_day().total_microseconds());
            // Calculate the day from today that will wake
            // If the wake time is before now, it will be tomorrow, at least
            int daysDelta = wake < nowDelta ? 1 : 0;
            greg_weekday today = (now.to_posix_time().date() + days(daysDelta)).day_of_week();
            if (weekdays) {
                switch (today) {
                case Saturday: daysDelta++;
                case Sunday: daysDelta++;
                }
            } else {
                switch (today) {
                case Monday: daysDelta++;
                case Tuesday: daysDelta++;
                case Wednesday: daysDelta++;
                case Thursday: daysDelta++;
                case Friday: daysDelta++;
                }
            }

            return now - nowDelta + wake + Duration(daysDelta * 86400.0);
        }
    };

    void generateWorkload(uint32_t u) {
        User & user = users[u];
        unsigned int batchSize = batchCDF.inverse(Simulator::uniform01());
        Duration when(0.0);
        LogMsg("Sim.Site", INFO) << "User " << u << " creates a batch of size " << batchSize;
        SimAppDatabase & sdb = sim.getNode(u).getDatabase();
        for (unsigned int i = 0; i < batchSize; i++) {
            if (!user.repeat) {
                user.lastAppRT = sim.getCurrentTime() + when;
                user.lastAppCmd = rg.generate(sim.getNode(u), user.lastAppRT);
                user.lastApp = sdb.getLastApp();
                user.repeat = repeatCDF.inverse(Simulator::uniform01());
            } else {
                // Repeat app
                sdb.createAppDescription(user.lastApp.first, user.lastApp.second);
                // Update release time and deadline to the new release time 'when'
                Duration d = user.lastAppCmd->getDeadline() - user.lastAppRT;
                user.lastAppRT = sim.getCurrentTime() + when;
                user.lastAppCmd->setDeadline(user.lastAppRT + d);
            }
            user.repeat--;
            LogMsg("Sim.Site", INFO) << "   Sending app " << *user.lastAppCmd << " at " << user.lastAppRT;
            sim.injectMessage(u, u, shared_ptr<BasicMsg>(user.lastAppCmd->clone()), when);
            when += Duration(interBatchTimeCDF.inverse(Simulator::uniform01()));
        }
        user.state = User::WAIT_JOB_FINISH;
    }

    void generateThinkTime(uint32_t u, Duration rt) {
        Duration tt;
        if (Simulator::uniform01() <= 0.8 / ((0.05 * rt.seconds()) / 60.0 + 1.0)) {
            // Calculate think time
            tt = Duration(thinkTimeCDF.inverse(Simulator::uniform01()));
            LogMsg("Sim.Site", INFO) << "User " << u << " thinks for " << tt << " seconds";
        } else {
            // Calculate break
            tt = Duration(breakTimeCDF.inverse(Simulator::uniform01()));
            LogMsg("Sim.Site", INFO) << "User " << u << " breaks for " << tt << " seconds";
        }
        sim.setTimer(u, sim.getCurrentTime() + tt, timer);
        users[u].state = User::WAIT_TT;
    }

    void sleep(uint32_t u) {
        users[u].state = User::SLEEPING;
        Time wakeTime = users[u].getWakeTime(sim.getCurrentTime());
        sim.setTimer(u, wakeTime, timer);
        LogMsg("Sim.Site", INFO) << "User " << u << " sleeps until " << wakeTime;
    }

    CDF thinkTimeCDF, breakTimeCDF;
    CDF repeatCDF, batchCDF, interBatchTimeCDF;
    vector<User> users;
    RequestGenerator rg;
    double maxTime;
    double deadlineMultiplier;

public:
    siteLevel(const Properties & p) : SimulationCase(p), rg(p) {
        // Prepare the properties
    }

    static const string getName() { return string("siteLevel"); }

    void preStart() {
        // Before running simulation
        maxTime = property("max_sim_time", 0.0);

        // Data distributions
        thinkTimeCDF.loadFrom(fs::path(property("think_time_distribution", string("traces/thinktime.cdf"))));
        breakTimeCDF.loadFrom(fs::path(property("break_time_distribution", string("traces/breaktime.cdf"))));
        repeatCDF.loadFrom(fs::path(property("job_repeat_distribution", string("traces/jobrepeat.cdf"))));
        batchCDF.loadFrom(fs::path(property("batch_width_distribution", string("traces/batchwidth.cdf"))));
        interBatchTimeCDF.loadFrom(fs::path(property("interbatch_time_distribution", string("traces/interbatchtime.cdf"))));
        deadlineMultiplier = property("deadline_mult", 1.0);

        // Create users
        users.resize(sim.getNumNodes());
        for (uint32_t u = 0; u < users.size(); u++) {
            users[u].setup();
            if (users[u].isWtime(sim.getCurrentTime())) {
                generateThinkTime(u, Duration(0.0));
            } else {
                sleep(u);
            }
        }
    }

    void afterEvent(const Simulator::Event & ev) {
        if (typeid(*ev.msg) == typeid(DispatchCommandMsg)) {
            User & u = users[ev.to];
            // Look for the last instance inserted
            u.lastInstance = SimAppDatabase::getLastInstance();
            // Remove the app description
            //sim.getCurrentNode().getDatabase().dropAppDescription(static_pointer_cast<DispatchCommandMsg>(ev.msg)->getAppName());
        }
        percent = maxTime > 0.0 ? (sim.getCurrentTime() - Time()).seconds() * 100.0 / maxTime : 0.0;
    }

    void beforeEvent(const Simulator::Event & ev) {
        if (typeid(*ev.msg) == typeid(UserEvent)) {
            User & u = users[ev.to];
            if (u.state == User::SLEEPING) {
                generateWorkload(ev.to);
            } else {
                // Only WAIT_TT possible
                if (u.isWtime(sim.getCurrentTime())) {
                    generateWorkload(ev.to);
                } else {
                    sleep(ev.to);
                }
            }
        } else if (typeid(*ev.msg) == typeid(AppFinishedMsg)) {
            User & u = users[ev.to];
            long int appId = static_pointer_cast<AppFinishedMsg>(ev.msg)->getAppId();
            SimAppDatabase & sdb = sim.getCurrentNode().getDatabase();
            if (appId == u.lastInstance) {
                // Batch is finished
                if (u.isWtime(sim.getCurrentTime())) {
                    Duration rt = ev.t - sdb.getAppInstance(appId).rtime;
                    generateThinkTime(ev.to, rt);
                } else {
                    sleep(ev.to);
                }
            }
        } else if (typeid(*ev.msg) == typeid(RequestTimeout)) {
            // Increase deadline 20%
            long int reqId = static_pointer_cast<RequestTimeout>(ev.msg)->getRequestId();
            SimAppDatabase & sdb = sim.getCurrentNode().getDatabase();
            long int appId = sdb.getAppId(reqId);
            if (appId != -1) {
                const SimAppDatabase::AppInstance & app = sdb.getAppInstance(appId);
                double d = (app.req.getDeadline() - app.ctime).seconds();
                sdb.updateDeadline(appId, ev.t + Duration(d * deadlineMultiplier));
            }
        }
    }

    bool blockMessage(uint32_t src, uint32_t dst, const shared_ptr<BasicMsg> & msg) {
        // Ignore fail tolerance
        if (msg->getName() == "HeartbeatTimeout") return true;
        else if (msg->getName() == "MonitorTimer") return true;
        return false;
    }

    void postEnd() {
//		// Dump memory database to disk
//		fs::path statDir(property("results_dir", std::string("./results")));
//		if (property("memory_db", true))
//			saveDb(TaskBagAppDatabase::getInstance().getDatabase().getDatabase(), (statDir / fs::path("stars.db")).file_string().c_str());
    }
};
REGISTER_SIMULATION_CASE(siteLevel);

Duration siteLevel::User::morning((7*60 + 30) * 60.0), siteLevel::User::night((17*60 + 30) * 60.0);
shared_ptr<siteLevel::UserEvent> siteLevel::timer(new UserEvent);
