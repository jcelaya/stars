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

#include <vector>
#include <list>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "Logger.hpp"
#include "SimulationCase.hpp"
#include "Simulator.hpp"
#include "Time.hpp"
#include "DispatchCommandMsg.hpp"
#include "TaskBagAppDatabase.hpp"
#include "../sim/SimTask.hpp"
#include "RequestGenerator.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "RequestTimeout.hpp"
#include "TimeConstraintInfo.hpp"
#include "../sim/Distributions.hpp"
using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::gregorian;


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
        Simulator & sim = Simulator::getInstance();
        // Before running simulation

        // Simulation limit
        numInstances = property("num_searches", 1);
        LogMsg("Sim.Progress", 0) << "Performing " << numInstances << " searches.";

        // Global variables
        meanTime = property("mean_time", 60.0);
        generateTraceOnly = property("generate_trace_only", false);

        // Send first request
        uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
        shared_ptr<DispatchCommandMsg> dcm(rg.generate(sim.getNode(client), Time()));
        // Send this message to the client
        sim.injectMessage(client, client, dcm, Duration());
        nextInstance = 1;
        remainingApps = 0;
    }

    void beforeEvent(uint32_t src, uint32_t dst, const BasicMsg & msg) {
        if (typeid(msg) == typeid(DispatchCommandMsg) && nextInstance < numInstances) {
            // Calculate next send
            Simulator & sim = Simulator::getInstance();
            Duration timeToNext(Simulator::exponential(meanTime));

            uint32_t client = Simulator::uniform(0, sim.getNumNodes() - 1);
            shared_ptr<DispatchCommandMsg> dcm(rg.generate(sim.getNode(client), sim.getCurrentTime() + timeToNext));
            // Send this message to the client
            sim.injectMessage(client, client, dcm, timeToNext);
            nextInstance++;
        }
    }

    virtual void finishedApp(int64_t appId) {
        Simulator::getInstance().getCurrentNode().getDatabase().appInstanceFinished(appId);
        percent = (remainingApps++) * 100.0 / numInstances;
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
        uint32_t client;
        Time release;
        Time deadline;
        TaskDescription minReq;
        bool operator<(const AppInstance & r) const { return release < r.release; }
    };

    unsigned int numSearches, finishedApps;
    list<AppInstance> apps;

public:
    repeat(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const string getName() { return string("repeat"); }

    void preStart() {
        Simulator & sim = Simulator::getInstance();
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
                size_t firstpos = 0, lastpos = nextLine.find_first_of(',');
                // Ignore instance number
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
                r.minReq.setNumTasks(numTasks);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get task length
                unsigned long int length;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> length;
                r.minReq.setLength(length);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get task memory
                unsigned int mem;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> mem;
                r.minReq.setMaxMemory(mem);
                firstpos = lastpos + 1;
                lastpos = nextLine.find_first_of(',', firstpos);
                // Get task disk
                unsigned int disk;
                istringstream(nextLine.substr(firstpos, lastpos - firstpos)) >> disk;
                r.minReq.setMaxDisk(disk);
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
                r.deadline = Time(deadline * 1000000);
                // Other params
                r.minReq.setInputSize(property("task_input_size", 0));
                r.minReq.setOutputSize(property("task_output_size", 0));
                // Add request to the list
                apps.push_back(r);
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
        sim.getNode(r.client).getDatabase().setNextApp(r.minReq);
        shared_ptr<DispatchCommandMsg> dcm(new DispatchCommandMsg);
        dcm->setDeadline(r.deadline);
        sim.injectMessage(r.client, r.client, dcm, r.release - Time());
        finishedApps = 0;
    }

    void afterEvent(uint32_t src, uint32_t dst, const BasicMsg & msg) {
        if (typeid(msg) == typeid(DispatchCommandMsg)) {
            Simulator & sim = Simulator::getInstance();
            if (!apps.empty()) {
                AppInstance r = apps.front();
                apps.pop_front();
                // Send this message to the client
                sim.getNode(r.client).getDatabase().setNextApp(r.minReq);
                shared_ptr<DispatchCommandMsg> dcm(new DispatchCommandMsg);
                dcm->setDeadline(r.deadline);
                sim.injectMessage(r.client, r.client, dcm, r.release - sim.getCurrentTime());
            }
        }
    }

    virtual void finishedApp(int64_t appId) {
        Simulator::getInstance().getCurrentNode().getDatabase().appInstanceFinished(appId);
        percent = (finishedApps++) * 100.0 / numSearches;
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

        EMPTY_MSGPACK_DEFINE();
    };
    static shared_ptr<UserEvent> timer;

    class SendJobEvent : public BasicMsg {
    public:
        MESSAGE_SUBCLASS(SendJobEvent);

        EMPTY_MSGPACK_DEFINE();
    };
    static shared_ptr<SendJobEvent> sendCmd;

    struct User {
        enum {
            WAIT_JOB_FINISH,
            SLEEPING,
            WAIT_TT
        } state;
        double perceivedSpeed;
        unsigned int batchSize;
        unsigned int repeat;
        bool daytime, weekdays;
        Duration wDelta;
        static Duration morning, night;

        void setup(double p) {
            repeat = 0;
            daytime = Simulator::uniform01() > 0.3;
            weekdays = Simulator::uniform01() > 0.2;
            wDelta = Duration(Simulator::uniform(-3600.0, 3600.0));
            perceivedSpeed = p;
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

        Duration getWakeTime(Time now) const {
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

            return wake + Duration(daysDelta * 86400.0) - nowDelta;
        }
    };

    void generateWorkload(uint32_t u) {
        Simulator & sim = Simulator::getInstance();
        User & user = users[u];
        user.batchSize = batchCDF.inverse(Simulator::uniform01());
        Duration when(0.0);
        LogMsg("Sim.Site", INFO) << "User " << u << " creates a batch of size " << user.batchSize;
        for (unsigned int i = 0; i < user.batchSize; i++) {
            sim.injectMessage(u, u, sendCmd, when);
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
        Simulator::getInstance().injectMessage(u, u, timer, tt);
        users[u].state = User::WAIT_TT;
    }

    void sleep(uint32_t u) {
        users[u].state = User::SLEEPING;
        Simulator & sim = Simulator::getInstance();
        Duration wakeTime = users[u].getWakeTime(sim.getCurrentTime());
        sim.injectMessage(u, u, timer, wakeTime);
        LogMsg("Sim.Site", INFO) << "User " << u << " sleeps until " << wakeTime;
    }

    CDF thinkTimeCDF, breakTimeCDF;
    CDF repeatCDF, batchCDF, interBatchTimeCDF;
    vector<User> users;
    RequestGenerator rg;
    double maxTime;
    double deadlineMultiplier;

public:
    siteLevel(const Properties & p) : SimulationCase(p), rg(p),
            maxTime(p("max_sim_time", 0.0)), deadlineMultiplier(p("deadline_mult", 1.0)) {
        // Prepare the properties
    }

    static const string getName() { return string("siteLevel"); }

    void preStart() {
        Simulator & sim = Simulator::getInstance();
        // Before running simulation
        // Data distributions
        fs::path cdfpath(property("traces_path", string("share/oldsim/traces")));
        thinkTimeCDF.loadFrom(cdfpath / fs::path(property("think_time_distribution", string("thinktime.cdf"))));
        breakTimeCDF.loadFrom(cdfpath / fs::path(property("break_time_distribution", string("breaktime.cdf"))));
        repeatCDF.loadFrom(cdfpath / fs::path(property("job_repeat_distribution", string("jobrepeat.cdf"))));
        batchCDF.loadFrom(cdfpath / fs::path(property("batch_width_distribution", string("batchwidth.cdf"))));
        interBatchTimeCDF.loadFrom(cdfpath / fs::path(property("interbatch_time_distribution", string("interbatchtime.cdf"))));

        // Create users
        users.resize(sim.getNumNodes());
        for (uint32_t u = 0; u < users.size(); u++) {
            users[u].setup(sim.getNode(u).getAveragePower());
            if (users[u].isWtime(sim.getCurrentTime())) {
                generateThinkTime(u, Duration(0.0));
            } else {
                sleep(u);
            }
        }
    }

    void afterEvent(uint32_t src, uint32_t dst, const BasicMsg & msg) {
        percent = maxTime > 0.0 ? (Time::getCurrentTime() - Time()).seconds() * 100.0 / maxTime : 0.0;
    }

    void beforeEvent(uint32_t src, uint32_t dst, const BasicMsg & msg) {
        //if (typeid(msg) == typeid(UserEvent)) {
        if (&msg == timer.get()) {
            User & u = users[dst];
            if (u.state == User::SLEEPING) {
                generateWorkload(dst);
            } else {
                // Only WAIT_TT possible
                if (u.isWtime(Time::getCurrentTime())) {
                    generateWorkload(dst);
                } else {
                    sleep(dst);
                }
            }
        //} else if (typeid(msg) == typeid(SendJobEvent)) {
        } else if (&msg == sendCmd.get()) {
            User & u = users[dst];
            Simulator & sim = Simulator::getInstance();
            shared_ptr<DispatchCommandMsg> dcm;
            Time now = sim.getCurrentTime();
            if (!u.repeat) {
                dcm.reset(rg.generate(sim.getNode(dst), now));
                u.repeat = repeatCDF.inverse(Simulator::uniform01());
            } else {
                // Repeat app
                dcm.reset(new DispatchCommandMsg);
            }
            // Calculate deadline
            uint64_t w = sim.getNode(dst).getDatabase().getNextApp().getAppLength();
            // Max time: local computing time
            double max = w / sim.getNode(dst).getAveragePower();
            // Min time: perceived platform time
            double min = w * deadlineMultiplier / u.perceivedSpeed;
            // Ref time: time to break
            double ref = 1200.0 * (0.8 / Simulator::uniform01() - 1.0);
            if (ref < min) ref = min;
            if (ref > max) ref = max;
            Time deadline = now + Duration(ref);
            if (!u.isWtime(deadline))
                deadline = now + u.getWakeTime(deadline);
            dcm->setDeadline(deadline);
            sim.injectMessage(dst, dst, dcm);
            u.repeat--;
//        } else if (typeid(msg) == typeid(RequestTimeout)) {
//            // Increase deadline 20%
//            long int reqId = static_cast<const RequestTimeout &>(msg).getRequestId();
//            SimAppDatabase & sdb = Simulator::getInstance().getCurrentNode().getDatabase();
//            long int appId = sdb.getAppId(reqId);
//            if (appId != -1) {
//                const SimAppDatabase::AppInstance & app = sdb.getAppInstance(appId);
//                double d = (app.req.getDeadline() - app.ctime).seconds();
//                sdb.updateDeadline(appId, Simulator::getInstance().getCurrentTime() + Duration(d * 2));
//            }
        }
    }

    virtual void finishedApp(int64_t appId) {
        Simulator & sim = Simulator::getInstance();
        uint32_t dst = sim.getCurrentNode().getLocalAddress().getIPNum();
        User & u = users[dst];
        SimAppDatabase & sdb = sim.getCurrentNode().getDatabase();
        Duration rt = sim.getCurrentTime() - sdb.getAppInstance(appId)->ctime;
        u.perceivedSpeed *= 0.8;
        u.perceivedSpeed += 0.2 * sdb.getAppInstance(appId)->req.getAppLength() / rt.seconds();
        if (--u.batchSize == 0) {
            // Batch is finished
            if (u.isWtime(sim.getCurrentTime())) {
                generateThinkTime(dst, rt);
            } else {
                sleep(dst);
            }
        }
        sdb.appInstanceFinished(appId);
    }

//    bool blockMessage(uint32_t src, uint32_t dst, const shared_ptr<BasicMsg> & msg) {
//        // Ignore fail tolerance
//        if (msg->getName() == "HeartbeatTimeout") return true;
//        else if (msg->getName() == "MonitorTimer") return true;
//        return false;
//    }
//
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
shared_ptr<siteLevel::SendJobEvent> siteLevel::sendCmd(new SendJobEvent);