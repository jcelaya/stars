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

#ifndef COMMLAYER_H_
#define COMMLAYER_H_

#include <vector>
#include <queue>
#include <utility>
#include <boost/thread/condition.hpp>
#include <boost/shared_ptr.hpp>
#include "CommAddress.hpp"
#include "BasicMsg.hpp"
#include "Time.hpp"
#include "NetworkManager.hpp"


class CommLayer;

/**
 * \brief A service for message handling.
 *
 * This interface must be implemented by any component which wishes to handle
 * a certain kind of message. After registering with the CommLayer, it will
 * receive all the messages, so that it can select which ones to handle.
 */
class Service {
public:
    virtual ~Service() {}

protected:
    friend class CommLayer;
    virtual bool receiveMessage(const CommAddress & src, const BasicMsg & msg) = 0;
};


/**
 * \brief Communications layer.
 *
 * Communications layer. This class implements a communication layer for services.
 * Normal usage would be:
 *   - Initialize app.
 *   - Create desired services.
 *   - Register services with CommLayer singleton.
 *   - Start additional threads which inject messages into the CommLayer.
 *   - Start event loop.
 */
class CommLayer {
public:
    /**
     * Obtains the singleton instance of CommLayer
     */
    static CommLayer & getInstance();

    /**
     * Register a service with the communication layer, so that it can receive messages.
     * @param c The newly registered service.
     */
    void registerService(Service * c) {
        services.push_back(c);
    }

    /**
     * Unregisters a service which is being destroyed.
     * @param c The leaving service.
     */
    void unregisterService(Service * c) {
        for (std::vector<Service *>::iterator i = services.begin(); i != services.end(); i++)
            if (*i == c) {
                services.erase(i);
                break;
            }
    }

    /**
     * Opens a new socket to listen to incoming network connections.
     * The port is the one specified by ConfigurationManager.
     */
    void listen() {
        nm->listen();
    }

    /**
     * Takes the next message from the queue and relays it to the services.
     */
    void processNextMessage();

    /**
     * Simple event loop.
     */
    void commEventLoop() {
        exitSignaled = false;
        while (!exitSignaled) processNextMessage();
    }

    void stopEventLoop();

    /**
     * Checks whether the queue is empty
     * @return True when there are messages in the queue.
     */
    bool availableMessages() const {
        return !messageQueue.empty();
    }

    /**
     * Sends a message. If the destination is the local host, then queue the message; otherwise,
     * send it through the network.
     * @param dst Destination address. It may be the local address, if the destination service resides in
     *            this host.
     * @param msg Message that is being sent. Ownership is kept by sender service.
     * @return The size of the sent message, in bytes.
     */
    unsigned int sendMessage(const CommAddress & dst, BasicMsg * msg);

    unsigned int sendLocalMessage(BasicMsg * msg) {
        return sendMessage(localAddress, msg);
    }

    /**
     * Returns the local address of this host.
     * @return The local address of this host.
     */
    const CommAddress & getLocalAddress() const {
        return localAddress;
    }

    /**
     * Prepares a message to be sent when a certain timer expires. The service receiving it will be that
     * stated by the message's service ID.
     * @param time The moment at which the message will be sent.
     * @param msg The message to be sent.
     * @return An ID of the timer, so it can be cancelled or rescheduled.
     */
    int setTimer(Time time, BasicMsg * msg) {
        boost::shared_ptr<BasicMsg> tmp(msg);
        if (time > Time::getCurrentTime()) return setTimerImpl(time, tmp);
        else return 0;
    }

    /**
     * Prepares a message to be sent when a certain timer expires. The service receiving it will be that
     * stated by the message's service ID.
     * @param delay The delay after which the message will be sent.
     * @param msg The message to be sent.
     * @return An ID of the timer, so it can be cancelled or rescheduled.
     */
    int setTimer(Duration delay, BasicMsg * msg) {
        boost::shared_ptr<BasicMsg> tmp(msg);
        if (!delay.is_negative()) return setTimerImpl(Time::getCurrentTime() + delay, tmp);
        else return 0;
    }

    /**
     * Cancels an already scheduled timer.
     * @param timerId ID of the timer to be canceled;
     */
    void cancelTimer(int timerId);

protected:
    friend class NetworkManager;

    /// Avoids instantiation
    CommLayer();

    /**
     * Enqueues a message to be processed by processNextMessage.
     * @param src Source address. It may be the local address, if the source service resides in
     *            this host.
     * @param msg Message that is being received.
     */
    void enqueueMessage(const CommAddress & src, const boost::shared_ptr<BasicMsg> & msg);

    /// Adds a new timer without checking if time is greater than current time
    int setTimerImpl(Time time, boost::shared_ptr<BasicMsg> msg);

    /**
     * Checks the list for expired timers and sends the corresponding message to the CommLayer
     */
    void checkExpired();

    boost::scoped_ptr<NetworkManager> nm;

    typedef std::pair<CommAddress, boost::shared_ptr<BasicMsg> > AddrMsg;
    /// Registered services
    std::vector<Service *> services;
    std::list<AddrMsg> messageQueue;    ///< Queue of received messages
    boost::mutex queueMutex;            ///< Mutex to put and get queue messages.
    boost::condition nonEmptyQueue;     ///< Barrier to wait for a message
    bool exitSignaled;                  ///< True when a SIGINT arrives, to exit the event loop

    /**
     * A timer, which delivers a message at a specific time.
     */
    struct Timer {
        static int timerId;   ///< ID counter

        Time timeout;              ///< Time at which the message is to be delivered
        boost::shared_ptr<BasicMsg> msg;   ///< Message to be delivered
        int id;                     ///< ID of this timer

        /// Constructor, from a time and a message
        Timer(Time t, boost::shared_ptr<BasicMsg> m) : timeout(t), msg(m), id(++timerId) {}

        /**
        * Less operator.
        * @param r Right operand.
        * @return True if this timer is earlier than r.
        */
        bool operator<(const Timer & r) {
            return timeout < r.timeout;
        }
    };

    std::list<Timer> timerList;   ///< List of timers in timeout order
    boost::mutex timerMutex;      ///< Object access mutex

    CommAddress localAddress;   ///< Local address of this node

private:
    // Avoids copy
    CommLayer(const CommLayer &);
    CommLayer & operator=(const CommLayer &);
};

#endif /*COMMLAYER_H_*/
