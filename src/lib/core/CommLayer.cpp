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

#include <signal.h>
#include "Logger.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
#include "CommLayer.hpp"
using boost::mutex;


int CommLayer::Timer::timerId = 0;


static void intTrap(int) {
    CommLayer::getInstance().stopEventLoop();
}


CommLayer::CommLayer() : nm(new NetworkManager), exitSignaled(false) {
    localAddress = nm->getLocalAddress();
    Logger::msg("Comm", DEBUG, "Local address is ", localAddress);
    signal(SIGINT, intTrap);
}


CommLayer & CommLayer::getInstance() {
    static CommLayer instance;
    return instance;
}


void CommLayer::stopEventLoop() {
    {
        mutex::scoped_lock lock(queueMutex);
        exitSignaled = true;
    }
    nonEmptyQueue.notify_all();
}


void CommLayer::processNextMessage() {
    CommAddress src;
    std::shared_ptr<BasicMsg> msg;
    {
        mutex::scoped_lock lock(queueMutex);
        // Wait until there are messages in the queue
        if (messageQueue.empty()) nonEmptyQueue.wait(lock);
        if (exitSignaled) return;
        src = messageQueue.front().first;
        msg = messageQueue.front().second;
        messageQueue.pop_front();
    }

    // Look for all the registered components
    bool isHandled = false;
    Logger::msg("Comm", DEBUG, "Processing message ", *msg);
    for (std::vector<Service *>::iterator it = services.begin(); it != services.end(); it++) {
        isHandled |= (*it)->receiveMessage(src, *msg);
    }

    if (!isHandled && src != localAddress) {
        // It's not critical to receive a message with no handler
        Logger::msg("Comm", WARN, "No handler for message of type ", msg->getName());
    }
}


void CommLayer::enqueueMessage(const CommAddress & src, const std::shared_ptr<BasicMsg> & msg) {
    {
        mutex::scoped_lock lock(queueMutex);
        messageQueue.push_back(AddrMsg(src, msg));
    }
    nonEmptyQueue.notify_all();
}


unsigned int CommLayer::sendMessage(const CommAddress & dst, BasicMsg * msg) {
    if (dst == getLocalAddress()) {
        enqueueMessage(dst, std::shared_ptr<BasicMsg>(msg));
        return 0;
    } else {
        std::unique_ptr<BasicMsg> tmp(msg);
        return nm->sendMessage(dst, msg);
    }
}


int CommLayer::setTimerImpl(Time time, std::shared_ptr<BasicMsg> msg) {
    // Add a task to the timer structure
    Timer t(time, msg);
    {
        mutex::scoped_lock lock(timerMutex);
        timerList.push_front(t);
        timerList.sort();
        nm->setAsyncTimer(timerList.front().timeout);
    }
    return t.id;
}


void CommLayer::cancelTimer(int timerId) {
    mutex::scoped_lock lock(timerMutex);
    // Erase timer in hashmap and list
    for (std::list<Timer>::iterator it = timerList.begin(); it != timerList.end(); it++) {
        if (it->id == timerId) {
            Logger::msg("Time", DEBUG, "Erasing timer with id ", timerId);
            timerList.erase(it);
            break;
        }
    }
}


void CommLayer::checkExpired() {
    Time ct = Time::getCurrentTime();
    mutex::scoped_lock lock(timerMutex);
    while (!timerList.empty()) {
        if (timerList.front().timeout <= ct) {
            enqueueMessage(localAddress, timerList.front().msg);
            timerList.pop_front();
        } else {
            // Program next timer
            nm->setAsyncTimer(timerList.front().timeout);
            break;
        }
    }
}
