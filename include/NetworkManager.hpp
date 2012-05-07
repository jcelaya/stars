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

#ifndef NETWORKMANAGER_H_
#define NETWORKMANAGER_H_

#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
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
		boost::asio::ip::tcp::socket socket;            ///< Socket connecting with the other node
		boost::asio::streambuf readBuffer;    ///< Read buffer
		boost::asio::streambuf writeBuffer;   ///< Write buffer
		static const std::size_t maxReadBufferSize = 1000000;
		Connection(boost::asio::io_service & io) : socket(io), readBuffer(maxReadBufferSize) {}
	};

	// Non-copyable
	NetworkManager(const NetworkManager &);
	NetworkManager & operator=(const NetworkManager &);

	/*
	 * Handler for the sending of a message
	 */
	void handleWrite(const boost::system::error_code & error, boost::shared_ptr<Connection> c);

	/**
	 * Handler for an accept on the incoming socket
	 */
	void handleAccept(const boost::system::error_code & error);

	/**
	 * Handler for the arrival of data
	 */
	void handleRead(const boost::system::error_code & error, std::size_t bytes_transferred, boost::shared_ptr<Connection> c);

	boost::scoped_ptr<boost::thread> t;                 ///< Thread for the handling of asynchronous events
	boost::asio::io_service io;                        ///< IO object from asio lib
	boost::asio::ip::tcp::acceptor acceptor;               ///< Acceptor for incoming connections
	boost::shared_ptr<Connection> incoming;      ///< Socket for an incoming connection

	boost::asio::deadline_timer timer;                 ///< Timer for the TimerManager events
};

#endif /* NETWORKMANAGER_H_ */
