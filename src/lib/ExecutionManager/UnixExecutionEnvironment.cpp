/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya, María Ángeles Giménez
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

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include "UnixExecutionEnvironment.hpp"
#include "ConfigurationManager.hpp"
#include "Task.hpp"
#include "TaskStateChgMsg.hpp"
#include "Logger.hpp"
using namespace boost;
using namespace std;


class UnixProcess : public Task {
    pid_t pid;
    mutex m;
    mutex::scoped_lock runlock;
    int status;

    void prepareAndRun();

public:
    /**
     * Creates a Task object.
     * @param o The address of the task owner node.
     * @param reqId The ID of the request which this task arrived in.
     * @param ctId The ID of this task relative to its request.
     * @param d TaskDescription with the task requirements.
     */
    UnixProcess(CommAddress o, int64_t reqId, unsigned int ctid, const TaskDescription & d) :
            Task(o, reqId, ctid, d), pid(0), runlock(m), status(Inactive) {
        // Launch a thread to prepare task while waiting to be run
        try {
            thread thrdExe(bind(&UnixProcess::prepareAndRun, this));
        } catch (boost::thread_resource_error & e) {
            status = Aborted;
        }
    }

    ~UnixProcess() {
        abort();
    }

    /**
     * Returns the current status of this task.
     * @return Status.
     */
    int getStatus() const {
        return status;
    }

    pid_t getPid() const {
        return pid;
    }

    /**
     * Starts running this task.
     */
    void run() {
        LogMsg("Unix", DEBUG) << "Running task " << taskId;
        runlock.unlock();
    }

    /**
     * Aborts the execution of a task.
     */
    void abort() {
        if (pid != 0) {
            kill(pid, 15);
        }
    }

    /**
     * Returns the estimated duration of this task. It must take into account only
     * the remaining part of this task.
     * @return Estimated duration.
     */
    Duration getEstimatedDuration() const {
        // TODO
        return Duration(description.getLength() / 1000.0);
    }
};


void UnixProcess::prepareAndRun() {
    TaskStateChgMsg tscm;
    tscm.setTaskId(taskId);

    LogMsg("Unix", DEBUG) << "Preparing task " << taskId;

    //Donwload input and executable files

    LogMsg("Unix", DEBUG) << "Updating execution state to PREPARED";
    tscm.setOldState(status);
    status = Prepared;
    tscm.setNewState(status);
    CommLayer::getInstance().sendLocalMessage(tscm.clone());
    //sendStatus(et.getTaskId(), UICodes::PREPARED_EXECUTION_TASK);
    {
        // Wait until run() is called
        mutex::scoped_lock lock(m);
    }

    // TODO: Obtain parameters, executable name... and fork
// ostringstream osTask;
// osTask << idRequest;
// string idTaskStr (osTask.str());
//
// ostringstream osRelative;
// osRelative << idRelative;
// string idRelativeStr (osRelative.str());
//
// //Reading description file
// string pathTask(pathStatic + "Storage/" + idTaskStr + "%" + idRelativeStr);
// DescriptionFile df(pathTask);
//
// //Executing
// string command = df.getExecutable();
// chdir(pathTask.c_str());
// string execCall = "exec " + command;
    if ((pid = fork())) {
        LogMsg("Unix", DEBUG) << "Updating execution state to RUNNING";
        tscm.setOldState(status);
        status = Running;
        tscm.setNewState(status);
        CommLayer::getInstance().sendLocalMessage(tscm.clone());
        // Wait termination
        int retval;
        if (waitpid(pid, &retval, 0) < 0 || !WIFEXITED(retval) || WEXITSTATUS(retval) != 0) {
            LogMsg("Unix", WARN) << "Aborted task " << taskId;
            tscm.setOldState(status);
            status = Aborted;
            tscm.setNewState(status);
            CommLayer::getInstance().sendLocalMessage(tscm.clone());
        } else {
            LogMsg("Unix", INFO) << "Finished task " << taskId;
            tscm.setOldState(status);
            status = Finished;
            tscm.setNewState(status);
            CommLayer::getInstance().sendLocalMessage(tscm.clone());
        }
    } else {
        // Actually execute the task
        //int ret = execlp("/bin/sh", "sh", "-c", execCall.c_str(), (char*) 0);
        exit(127);
    }

    //Upload results
    //cxf.upload(df.getResult());
    LogMsg("Unix", DEBUG) << "End execution thread";
}


double UnixExecutionEnvironment::getAveragePower() const {
    // TODO
    return 1000.0;
}


unsigned long int UnixExecutionEnvironment::getAvailableMemory() const {
    return ConfigurationManager::getInstance().getAvailableMemory();
}


unsigned long int UnixExecutionEnvironment::getAvailableDisk() const {
    return ConfigurationManager::getInstance().getAvailableDisk();
}


shared_ptr<Task> UnixExecutionEnvironment::createTask(CommAddress o, int64_t reqId, unsigned int ctid, const TaskDescription & d) const {
    return shared_ptr<Task>(new UnixProcess(o, reqId, ctid, d));
}
