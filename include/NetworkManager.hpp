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

#ifndef NETWORKMANAGER_H_
#define NETWORKMANAGER_H_

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "Time.hpp"
#include "CommAddress.hpp"
#include "BasicMsg.hpp"

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager() {
        if (t.get()) {
            io.stop();
            t->join();
        }
    }

    unsigned int sendMessage(const CommAddress & dst, BasicMsg * msg);

    CommAddress getLocalAddress() const;

    /**
     * Starts listening to incoming network connections. Call it only once.
     */
    void listen();

    /**
     * Sets an synchronous timer which calls checkExpired on trigger
     */
    void setAsyncTimer(Time timeout);

private:
    /**
     * A connection between this node and another one
     */
    struct Connection {
        CommAddress dst;
        boost::asio::ip::tcp::socket socket;            ///< Socket connecting with the other node
        boost::asio::streambuf readBuffer;    ///< Read buffer
        boost::asio::streambuf writeBuffer;   ///< Write buffer
        static const std::size_t maxReadBufferSize = 1000000;
        Connection(boost::asio::io_service & io) : socket(io), readBuffer(maxReadBufferSize) {}
        ~Connection();
    };

    // Non-copyable
    NetworkManager(const NetworkManager &);
    NetworkManager & operator=(const NetworkManager &);

    /*
     * Handler for the connection with a remote node
     */
    void handleConnect(const boost::system::error_code & error, std::shared_ptr<Connection> c);

    /*
     * Handler for the sending of a message
     */
    void handleWrite(const boost::system::error_code & error, std::shared_ptr<Connection> c);

    /**
     * Handler for an accept on the incoming socket
     */
    void handleAccept(const boost::system::error_code & error);

    /**
     * Handler for the arrival of data
     */
    void handleRead(const boost::system::error_code & error, std::size_t bytes_transferred, std::shared_ptr<Connection> c);

    std::unique_ptr<boost::thread> t;                 ///< Thread for the handling of asynchronous events
    boost::asio::io_service io;                        ///< IO object from asio lib
    boost::asio::ip::tcp::acceptor acceptor;               ///< Acceptor for incoming connections
    std::shared_ptr<Connection> incoming;      ///< Socket for an incoming connection

    boost::asio::deadline_timer timer;                 ///< Timer for the TimerManager events
};

#endif /* NETWORKMANAGER_H_ */
