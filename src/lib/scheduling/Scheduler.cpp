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
//using boost::shared_ptr;


Scheduler::ExecutionEnvironmentImpl::ExecutionEnvironmentImpl() : impl(new UnixExecutionEnvironment) {}

void Scheduler::addedTasksEvent(const TaskBagMsg & msg, unsigned int numAccepted) {}
void Scheduler::startedTaskEvent(const Task & t) {}
void Scheduler::finishedTaskEvent(const Task & t, int oldState, int newState) {}


// Timers

class MonitorTimer : public BasicMsg {
public:
    MESSAGE_SUBCLASS(MonitorTimer);

    EMPTY_MSGPACK_DEFINE();
};
static boost::shared_ptr<MonitorTimer> monTmr(new MonitorTimer);
static boost::shared_ptr<RescheduleTimer> reschTmr(new RescheduleTimer);


/**
 * A finished task message, that signals the successful termination of a task.
 * It contains the id of the task that finished.
 *
 * It must remove the task from the queue and reschedule.
 * @param msg The TaskEventMsg.
 */
template<> void Scheduler::handle(const CommAddress & src, const TaskStateChgMsg & msg) {
    if (src == CommLayer::getInstance().getLocalAddress()) {
        Logger::msg("Ex.Sch", INFO, "Received a TaskStateChgMsg from task ", msg.getTaskId());
        Logger::msg("Ex.Sch", DEBUG, "   Task ", msg.getTaskId(), " changed state from ", msg.getOldState(), " to ", msg.getNewState());
        switch (msg.getNewState()) {
        case Task::Finished:
            ++tasksExecuted;
        case Task::Aborted: {
            // Remove the task from the queue
            bool notFound = true;
            for (auto i = tasks.begin(); i != tasks.end() && notFound; ++i)
                if ((*i)->getTaskId() == msg.getTaskId()) {
                    notFound = false;
                    // For statistics purpose
                    finishedTaskEvent(**i, msg.getOldState(), msg.getNewState());
                    // Send a TaskMonitorMsg to signal finalization
                    boost::shared_ptr<Task> task = *i;
                    TaskMonitorMsg * tmm = new TaskMonitorMsg;
                    tmm->addTask(task->getClientRequestId(), task->getClientTaskId(), msg.getNewState());
                    tmm->setHeartbeat(ConfigurationManager::getInstance().getHeartbeat());
                    CommLayer::getInstance().sendMessage(task->getOwner(), tmm);
                    removeTask(task);
                    tasks.erase(i);
                    break;
                }
            if (notFound) Logger::msg("Ex.Sch", ERROR, "Trying to remove a non-existent task!!");
            break;
        }
        case Task::Inactive:
        case Task::Prepared:
        case Task::Running:
            break;
        }
        reschedule();
        countPausedTasks();
        notifySchedule();
    }
}


/**
 * A request for a group of available nodes to assign a bag of tasks to.
 *
 * It is sent by the SubmissionNode, and then it is divided by each branch StructureNode.
 * @param src The source address.
 * @param msg The received message.
 */
template<> void Scheduler::handle(const CommAddress & src, const TaskBagMsg & msg) {
    // Check it is for us
    if (msg.isForEN()) {
        Logger::msg("Ex.Sch", INFO, "Handling TaskBagMsg from ", src);
        unsigned int numTasks = msg.getLastTask() - msg.getFirstTask() + 1;
        unsigned int numAccepted = 0;
        if (checkStaticRequirements(msg.getMinRequirements())) {
            // Take the TaskDescription object and try to accept it
            Logger::msg("Ex.Sch", INFO, "Accepting ", numTasks, " tasks from request ",
                    msg.getRequestId(), " for ", msg.getRequester());
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

                // For statistics purpose
                addedTasksEvent(msg, numAccepted);
            }

            if (numAccepted == numTasks) return;
        }
        // If control reaches this point, there are tasks which were not accepted.
        Logger::msg("Ex.Sch", WARN, (numTasks - numAccepted), " tasks rejected.");
    }
}


bool Scheduler::checkStaticRequirements(const TaskDescription & req) {
    if (req.getMaxMemory() > backend.impl->getAvailableMemory()) {
        Logger::msg("Ex.Sch", WARN, "Not enough memory to execute the task: ",
                req.getMaxMemory(), " > ", backend.impl->getAvailableMemory());
    } else if (req.getMaxDisk() > backend.impl->getAvailableDisk()) {
        Logger::msg("Ex.Sch", WARN, "Not enough disk to execute the task: ",
                req.getMaxDisk(), " > ", backend.impl->getAvailableDisk());
    } else return true;
    return false;
}


/**
 * A timer to signal that a reschedule is needed, in order to check deadlines or
 * provide the father with fresher information.
 */
template<> void Scheduler::handle(const CommAddress & src, const RescheduleTimer & msg) {
    rescheduleTimer = 0;
    reschedule();
    countPausedTasks();
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
        for (auto it = tasks.begin(); it != tasks.end() && notFound; ++it)
            if ((*it)->getClientRequestId() == msg.getRequestId() && (*it)->getClientTaskId() == msg.getTask(i)) {
                notFound = false;
                finishedTaskEvent(**it, (*it)->getStatus(), Task::Aborted);
                (*it)->abort();
                tasks.erase(it);
                break;
            }
        if (notFound) Logger::msg("Ex.Sch", ERROR, "Failed to remove non-existent task ", msg.getTask(i), " from request ", msg.getRequestId());
    }
    reschedule();
    countPausedTasks();
    notifySchedule();
}


template<> void Scheduler::handle(const CommAddress & src, const MonitorTimer & msg) {
    if (!tasks.empty()) {
        Logger::msg("Ex.Sch", INFO, "Sending monitoring reminders");
        map<CommAddress, TaskMonitorMsg *> dsts;
        for (list<boost::shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); i++) {
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
    if (dynamic_cast<const TaskBagMsg *>(&msg)) { handle(src, static_cast<const TaskBagMsg &>(msg)); return true; }
    HANDLE_MESSAGE(TaskStateChgMsg)
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


boost::shared_ptr<Task> Scheduler::getTask(unsigned int id) {
    // Check that the id exists
    for (list<boost::shared_ptr<Task> >::iterator i = tasks.begin(); i != tasks.end(); i++)
        if ((*i)->getTaskId() == id) return *i;
    Logger::msg("Ex.Sch", ERROR, "Trying to get a non-existent task!!");
    return boost::shared_ptr<Task>();
}


void Scheduler::notifySchedule() {
    Logger::msg("Ex.Sch", DEBUG, "Setting attributes to ", getAvailability());
    if (!inChange && leaf.getFatherAddress() != CommAddress()) {
        AvailabilityInformation * msg = getAvailability().clone();
        msg->setSeq(++seqNum);
        CommLayer::getInstance().sendMessage(leaf.getFatherAddress(), msg);
        dirty = false;
    } else {
        Logger::msg("Ex.Sch", DEBUG, "Delayed sending info to father");
        dirty = true;
    }
}
