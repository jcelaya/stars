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
#include <map>
#include <cstdlib>
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "AcceptTaskMsg.hpp"
#include "AbortTaskMsg.hpp"
#include "TaskMonitorMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "RescheduleTimer.hpp"
#include "ConfigurationManager.hpp"
#include "UnixExecutionEnvironment.hpp"
using namespace std;
using boost::shared_ptr;


Scheduler::ExecutionEnvironmentImpl::ExecutionEnvironmentImpl() : impl(new UnixExecutionEnvironment) {}

void Scheduler::queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd) {}


// Timers

class MonitorTimer : public BasicMsg {
public:
    MESSAGE_SUBCLASS(MonitorTimer);

    EMPTY_MSGPACK_DEFINE();
};
static shared_ptr<MonitorTimer> monTmr(new MonitorTimer);
static shared_ptr<RescheduleTimer> reschTmr(new RescheduleTimer);


/**
 * A finished task message, that signals the successful termination of a task.
 * It contains the id of the task that finished.
 *
 * It must remove the task from the queue and reschedule.
 * @param msg The TaskEventMsg.
 */
template<> void Scheduler::handle(const CommAddress & src, const TaskStateChgMsg & msg) {
    if (src == CommLayer::getInstance().getLocalAddress()) {
        LogMsg("Ex.Sch", INFO) << "Received a TaskStateChgMsg from task " << msg.getTaskId();
        LogMsg("Ex.Sch", DEBUG) << "   Task " << msg.getTaskId() << " changed state from " << msg.getOldState() << " to "
        << msg.getNewState();
        switch (msg.getNewState()) {
        case Task::Finished:
            ++tasksExecuted;
        case Task::Aborted: {
            // Remove the task from the queue
            bool notFound = true;
            for (list<shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); ++i)
                if ((*i)->getTaskId() == msg.getTaskId()) {
                    notFound = false;
                    // Send a TaskMonitorMsg to signal finalization
                    shared_ptr<Task> task = *i;
                    TaskMonitorMsg * tmm = new TaskMonitorMsg;
                    tmm->addTask(task->getClientRequestId(), task->getClientTaskId(), msg.getNewState());
                    tmm->setHeartbeat(ConfigurationManager::getInstance().getHeartbeat());
                    CommLayer::getInstance().sendMessage(task->getOwner(), tmm);
                    removeTask(task);
                    tasks.erase(i);
                    break;
                }
            if (notFound) LogMsg("Ex.Sch", ERROR) << "Trying to remove a non-existent task!!";
            break;
        }
        case Task::Inactive:
        case Task::Prepared:
        case Task::Running:
            break;
        }
        reschedule();
        notifySchedule();
    }
}


/**
 * A request for a group of available nodes to assign a bag of tasks to. In this case, the
 * bag should have only one task, to be assigned to this Scheduler.
 *
 * It is sent firstly by the client, and then it is divided by each branch StructureNode.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void Scheduler::handle(const CommAddress & src, const TaskBagMsg & msg) {
    // Check it is for us
    if (msg.isForEN()) {
        LogMsg("Ex.Sch", INFO) << "Handling TaskBagMsg from " << src;
        unsigned int numTasks = msg.getLastTask() - msg.getFirstTask() + 1;
        unsigned int numAccepted = 0;
        const TaskDescription & desc = msg.getMinRequirements();
        // Check static constraints
        if (desc.getMaxMemory() > backend.impl->getAvailableMemory()) {
            LogMsg("Ex.Sch", DEBUG) << "Not enough memory to execute the task: " << desc.getMaxMemory() << " > " << backend.impl->getAvailableMemory();
        } else if (desc.getMaxDisk() > backend.impl->getAvailableDisk()) {
            LogMsg("Ex.Sch", DEBUG) << "Not enough disk to execute the task: " << desc.getMaxDisk() << " > " << backend.impl->getAvailableDisk();
        } else {
            // Take the TaskDescription object and try to accept it
            LogMsg("Ex.Sch", INFO) << "Accepting " << numTasks << " tasks from request " << msg.getRequestId() << " for " << msg.getRequester();
            numAccepted = accept(msg);
            if (numAccepted > 0) {
                notifySchedule();
                // Acknowledge the requester
                AcceptTaskMsg * atm = new AcceptTaskMsg;
                atm->setRequestId(msg.getRequestId());
                atm->setFirstTask(msg.getFirstTask());
                atm->setLastTask(atm->getFirstTask() + numAccepted - 1);
                atm->setHeartbeat(ConfigurationManager::getInstance().getHeartbeat());
                CommLayer::getInstance().sendMessage(msg.getRequester(), atm);
                if (monitorTimer == 0) setMonitorTimer();
            }

            // For statistics purpose
            Time queueEnd = Time::getCurrentTime();
            for (list<shared_ptr<Task> >::iterator it = tasks.begin(); it != tasks.end(); it++) {
                queueEnd += (*it)->getEstimatedDuration();
            }
            queueChangedStatistics(msg.getRequestId(), numAccepted, queueEnd);

            if (numAccepted == numTasks) return;
        }
        // If control reaches this point, there are tasks which were not accepted.
        LogMsg("Ex.Sch", WARN) << (numTasks - numAccepted) << " tasks rejected.";
    }
}


/**
 * A timer to signal that a reschedule is needed, in order to check deadlines or
 * provide the father with fresher information.
 */
template<> void Scheduler::handle(const CommAddress & src, const RescheduleTimer & msg) {
    rescheduleTimer = 0;
    reschedule();
    notifySchedule();
}


/**
 * A msg from the client that aborts a task
 * provide the father with fresher information.
 */
template<> void Scheduler::handle(const CommAddress & src, const AbortTaskMsg & msg) {
    for (unsigned int i = 0; i < msg.getNumTasks(); i++) {
        // Check that the id exists
        bool notFound = true;
        for (list<shared_ptr<Task> >::iterator it = tasks.begin(); it != tasks.end(); ++it)
            if ((*it)->getClientRequestId() == msg.getRequestId() && (*it)->getClientTaskId() == msg.getTask(i)) {
                notFound = false;
                (*it)->abort();
                tasks.erase(it);
                break;
            }
        if (notFound) LogMsg("Ex.Sch", ERROR) << "Failed to remove non-existent task " << msg.getTask(i) << " from request " << msg.getRequestId();
    }
    reschedule();
    notifySchedule();
}


template<> void Scheduler::handle(const CommAddress & src, const MonitorTimer & msg) {
    if (!tasks.empty()) {
        LogMsg("Ex.Sch", INFO) << "Sending monitoring reminders";
        map<CommAddress, TaskMonitorMsg *> dsts;
        for (list<shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); i++) {
            map<CommAddress, TaskMonitorMsg *>::iterator tmm =
                dsts.insert(dsts.begin(), make_pair((*i)->getOwner(), (TaskMonitorMsg *)NULL));
            if (tmm->second == NULL) tmm->second = new TaskMonitorMsg;
            tmm->second->addTask((*i)->getClientRequestId(), (*i)->getClientTaskId(), (*i)->getStatus());
        }
        for (map<CommAddress, TaskMonitorMsg *>::iterator i = dsts.begin(); i != dsts.end(); i++) {
            i->second->setHeartbeat(ConfigurationManager::getInstance().getHeartbeat());
            CommLayer::getInstance().sendMessage(i->first, i->second);
        }

        setMonitorTimer();
    } else monitorTimer = 0;
}


#define HANDLE_MESSAGE(x) if (typeid(msg) == typeid(x)) { handle(src, static_cast<const x &>(msg)); return true; }
bool Scheduler::receiveMessage(const CommAddress & src, const BasicMsg & msg) {
    HANDLE_MESSAGE(TaskStateChgMsg)
    HANDLE_MESSAGE(TaskBagMsg)
    HANDLE_MESSAGE(RescheduleTimer)
    HANDLE_MESSAGE(AbortTaskMsg)
    HANDLE_MESSAGE(MonitorTimer)
    return false;
}


void Scheduler::rescheduleAt(Time r) {
    if (rescheduleTimer != 0)
        CommLayer::getInstance().cancelTimer(rescheduleTimer);
    rescheduleTimer = CommLayer::getInstance().setTimer(r, reschTmr);
}


void Scheduler::setMonitorTimer() {
    // Randomize monitor timer with 10%
    monitorTimer = CommLayer::getInstance().setTimer(Duration((double)ConfigurationManager::getInstance().getHeartbeat()), monTmr);
}


shared_ptr<Task> Scheduler::getTask(unsigned int id) {
    // Check that the id exists
    for (list<shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); i++)
        if ((*i)->getTaskId() == id) return *i;
    LogMsg("Ex.Sch", ERROR) << "Trying to get a non-existent task!!";
    return shared_ptr<Task>();
}


void Scheduler::notifySchedule() {
    LogMsg("Ex.Sch", DEBUG) << "Setting attributes to " << getAvailability();
    if (!inChange && resourceNode.getFather() != CommAddress()) {
        AvailabilityInformation * msg = getAvailability().clone();
        msg->setSeq(++seqNum);
        CommLayer::getInstance().sendMessage(resourceNode.getFather(), msg);
        dirty = false;
    } else {
        LogMsg("Ex.Sch", DEBUG) << "Delayed sending info to father";
        dirty = true;
    }
}
