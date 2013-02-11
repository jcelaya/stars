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

#include <stdexcept>
#include "Logger.hpp"
#include "SubmissionNode.hpp"
#include "CommLayer.hpp"
#include "ResourceNode.hpp"
#include "AbortTaskMsg.hpp"
#include "ConfigurationManager.hpp"
#include "AcceptTaskMsg.hpp"
#include "TaskBagMsg.hpp"
#include "RequestTimeout.hpp"
#include "TaskMonitorMsg.hpp"
#include "Task.hpp"
using namespace std;


// Timers

class HeartbeatTimeout : public BasicMsg {
    CommAddress executionNode;

public:
    MESSAGE_SUBCLASS(HeartbeatTimeout);

    HeartbeatTimeout(const CommAddress & src) : executionNode(src) {}

    const CommAddress & getExecutionNode() const {
        return executionNode;
    }

    EMPTY_MSGPACK_DEFINE();
};


void SubmissionNode::finishedApp(int64_t appId) {}


void SubmissionNode::sendRequest(int64_t appInstance, int prevRetries) {
    if (!inChange) {
        // Prepare request message with all ready tasks
        TaskBagMsg * tbm = new TaskBagMsg;
        tbm->setLastTask(0);
        db.requestFromReadyTasks(appInstance, *tbm);
        if (tbm->getLastTask() == 0) {
            LogMsg("Sb", INFO) << "No more ready tasks for app instance " << appInstance;
            return;
        }
        int64_t reqId = tbm->getRequestId();
        tbm->setRequester(CommLayer::getInstance().getLocalAddress());
        tbm->setForEN(false);
        tbm->setFromEN(true);
        retries[reqId] = prevRetries + 1;
        remainingTasks[appInstance] += tbm->getLastTask() - tbm->getFirstTask() + 1;

        // Set a request timeout of 30 seconds
        Time timeout = Time::getCurrentTime() + Duration(30.0);
        // Schedule the timeout message
        boost::shared_ptr<RequestTimeout> rt(new RequestTimeout);
        rt->setRequestId(reqId);
        CommLayer::getInstance().setTimer(timeout, rt);
        if (db.startSearch(reqId, timeout)) {
            LogMsg("Sb", INFO) << "Sending request with " << (tbm->getLastTask() - tbm->getFirstTask() + 1) << " tasks of length "
                    << tbm->getMinRequirements().getLength() << " and deadline " << tbm->getMinRequirements().getDeadline();
            // Send this message to the father's Dispatcher
            CommLayer::getInstance().sendMessage(leaf.getFatherAddress(), tbm);
        }
        // On error, the request is sent again in 30 seconds
    } else {
        // Delay until the father node is stable again
        delayedInstances.push_back(make_pair(appInstance, prevRetries));
    }
}


/**
 * Handler for a submission command.
 * @param src Source address, should be local.
 * @param msg The command with the request information.
 */
template<> void SubmissionNode::handle(const CommAddress & src, const DispatchCommandMsg & msg) {
    LogMsg("Sb", INFO) << "Handling DispatchCommandMsg to dispatch an instance of app " << msg.getAppName();

    if (leaf.getFatherAddress() == CommAddress()) {
        LogMsg("Sb", ERROR) << "Trying to send an application request, but not in network...";
        return;
    }

    int64_t appId = db.createAppInstance(msg.getAppName(), msg.getDeadline());
    if (appId != -1) {
        remainingTasks[appId] = 0;
        sendRequest(appId, 0);
    } else
        LogMsg("Sb", ERROR) << "Application " << msg.getAppName() << " does not exist in database.";
}


void SubmissionNode::fatherChanged(bool changed) {
    inChange = false;
    // Send all unsent requests
    while (!delayedInstances.empty()) {
        sendRequest(delayedInstances.front().first, delayedInstances.front().second);
        delayedInstances.pop_front();
    }
}


/**
 * Handler for a task acceptance message
 * @param src Address of the execution node.
 * @param msg The msg with the accepted tasks identifiers.
 */
template<> void SubmissionNode::handle(const CommAddress & src, const AcceptTaskMsg & msg) {
    LogMsg("Sb", INFO) << "Handling AcceptTaskMsg for request " << msg.getRequestId()
    << ", tasks " << msg.getFirstTask() << " to " << msg.getLastTask() << " from " << src;

    // Reject all tasks that do not belong to this request?
//    AbortTaskMsg * atm = new AbortTaskMsg;
//    atm->setRequestId(msg.getRequestId());
//    for (unsigned int i = msg.getFirstTask(); i <= msg.getLastTask(); i++) {
//        if (!db.taskInRequest(i, msg.getRequestId())) {
//            LogMsg("Sb", DEBUG) << "Task " << i << " is not in this request, aborting";
//            atm->addTask(i);
//        }
//    }
//    unsigned int numTasks = atm->getNumTasks();
//    if (numTasks > 0)
//        CommLayer::getInstance().sendMessage(src, atm);
//    else delete atm;

    int64_t appId = db.getInstanceId(msg.getRequestId());
    if (appId != -1) {
        unsigned int numAccepted = db.acceptedTasks(src, msg.getRequestId(), msg.getFirstTask(), msg.getLastTask());
        if (numAccepted) {
            // Reset the number of retries for this instance
            retries[msg.getRequestId()] = 0;
            // Program a heartbeat timeout for this execution node if it does not exist yet
            int & timer = heartbeats.insert(make_pair(src, 0)).first->second;
            if (timer == 0) {
                timer = CommLayer::getInstance().setTimer(Duration(2.5 * msg.getHeartbeat()),
                        boost::shared_ptr<HeartbeatTimeout>(new HeartbeatTimeout(src)));
            }
            // Count tasks
            remoteTasks[src].insert(make_pair(appId, 0)).first->second += numAccepted;
        }
    } else {
        LogMsg("Sb", WARN) << "No application instance for request " << msg.getRequestId();
    }
}


/**
 * Handler for a request timeout
 * @param src Source address, should be local.
 * @param msg The msg with the accepted tasks identifiers.
 */
template<> void SubmissionNode::handle(const CommAddress & src, const RequestTimeout & msg) {
    int64_t appId = db.getInstanceId(msg.getRequestId());
    // Ignore a non-existent request
    if (appId != -1) {
        map<int64_t, int>::iterator prevRetries = retries.find(msg.getRequestId());
        // Change all SEARCHING tasks to READY
        map<int64_t, unsigned int>::iterator it = remainingTasks.find(appId);
        it->second -= db.cancelSearch(msg.getRequestId());
        if (db.getNumReady(appId) > 0
                && prevRetries->second < ConfigurationManager::getInstance().getSubmitRetries()) {
            LogMsg("Sb", WARN) << "Request " << msg.getRequestId() << " timed out with pending tasks.";
            // Start a new search
            sendRequest(appId, prevRetries->second);
        } else {
            if (it->second == 0) {
                finishedApp(it->first);
                remainingTasks.erase(it);
            }
        }
        retries.erase(prevRetries);
    }
}


template<> void SubmissionNode::handle(const CommAddress & src, const TaskMonitorMsg & msg) {
    LogMsg("Sb", INFO) << "Handling TaskMonitorMsg from node " << src;
    map<int64_t, unsigned int> & tasksPerApp = remoteTasks[src];

    for (unsigned int i = 0; i < msg.getNumTasks(); ++i) {
        LogMsg("Sb", INFO) << "Task " << msg.getTaskId(i) << " from request " << msg.getRequestId(i) << " is in state " << msg.getTaskState(i);

        if (!db.acceptedTasks(src, msg.getRequestId(i), msg.getTaskId(i), msg.getTaskId(i))) {
            // Task already accepted or non-existent
        }

        // Change state for finished tasks
        if (msg.getTaskState(i) == Task::Finished) {
            int64_t appId = db.getInstanceId(msg.getRequestId(i));
            if (db.finishedTask(src, msg.getRequestId(i), msg.getTaskId(i))) {
                map<int64_t, unsigned int>::iterator it = tasksPerApp.find(appId);
                if (it != tasksPerApp.end()) {
                    if (--(it->second) == 0) {
                        tasksPerApp.erase(it);
                    }
                    it = remainingTasks.find(appId);
                    if (it != remainingTasks.end() && --(it->second) == 0) {
                        finishedApp(it->first);
                        remainingTasks.erase(it);
                    }
                } else
                    LogMsg("Sb", WARN) << "Request " << msg.getRequestId(i) << " or appId " << appId << " do not exist";
            }
        } else if (msg.getTaskState(i) == Task::Aborted) {
            int64_t appId = db.getInstanceId(msg.getRequestId(i));
            if (db.abortedTask(src, msg.getRequestId(i), msg.getTaskId(i))) {
                remainingTasks[appId]--;
                map<int64_t, unsigned int>::iterator it = tasksPerApp.find(appId);
                if (it != tasksPerApp.end()) {
                    if (--(it->second) == 0) tasksPerApp.erase(it);
                    // Try to relaunch application
                    sendRequest(appId, 0);
                    LogMsg("Sb", WARN) << "Task " << msg.getTaskId(i) << " from request " << msg.getRequestId(i) << " from app " << appId << " aborted by remote.";
                } else
                    LogMsg("Sb", WARN) << "Request " << msg.getRequestId(i) << " or appId " << appId << " do not exist";
            }
        }
    }

    int & hb = heartbeats[src];
    CommLayer::getInstance().cancelTimer(hb);
    // If there are still any remote task in that execution node, reprogram a heartbeat timeout
    if (!tasksPerApp.empty())
        hb = CommLayer::getInstance().setTimer(Duration(2.5 * msg.getHeartbeat()),
                boost::shared_ptr<HeartbeatTimeout>(new HeartbeatTimeout(src)));
    else {
        remoteTasks.erase(src);
        heartbeats.erase(src);
    }
}


template<> void SubmissionNode::handle(const CommAddress & src, const HeartbeatTimeout & msg) {
    LogMsg("Sb", WARN) << "Execution node " << msg.getExecutionNode() << " is dead, relaunching tasks";
    heartbeats.erase(msg.getExecutionNode());
    // Set all the tasks being executed in that node to READY
    db.deadNode(msg.getExecutionNode());
    // Launch a new request for every failed application
    map<int64_t, unsigned int> & tasksPerApp = remoteTasks[msg.getExecutionNode()];
    for (map<int64_t, unsigned int>::iterator it = tasksPerApp.begin(); it != tasksPerApp.end(); it++) {
        remainingTasks[it->first] -= it->second;
        sendRequest(it->first, 0);
    }
    remoteTasks.erase(msg.getExecutionNode());
}


#define HANDLE_MESSAGE(x) if (typeid(msg) == typeid(x)) { handle(src, static_cast<const x &>(msg)); return true; }
bool SubmissionNode::receiveMessage(const CommAddress & src, const BasicMsg & msg) {
    HANDLE_MESSAGE(DispatchCommandMsg)
    HANDLE_MESSAGE(AcceptTaskMsg)
    HANDLE_MESSAGE(RequestTimeout)
    HANDLE_MESSAGE(TaskMonitorMsg)
    HANDLE_MESSAGE(HeartbeatTimeout)
    return false;
}
