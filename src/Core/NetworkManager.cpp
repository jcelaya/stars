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

#include <msgpack.hpp>
#include "NetworkManager.hpp"
#include "CommLayer.hpp"
#include "ConfigurationManager.hpp"
#include "Logger.hpp"
namespace net = boost::asio;
using boost::bind;
using net::ip::tcp;


NetworkManager::NetworkManager() : acceptor(io), incoming(new Connection(io)), timer(io) {}


void NetworkManager::listen() {
    // Start async accept before creating the thread. It will maintain the thread alive.
    acceptor.open(tcp::v4());
    acceptor.bind(tcp::endpoint(tcp::v4(), ConfigurationManager::getInstance().getPort()));
    acceptor.listen();
    acceptor.async_accept(incoming->socket,
                          bind(&NetworkManager::handleAccept, this, net::placeholders::error));
    t.reset(new boost::thread(bind(&net::io_service::run, &io)));
    LogMsg("Net", INFO) << "Thread " << t->get_id() << " accepting connections on port " << ConfigurationManager::getInstance().getPort();
    // The following is needed for the test cases, it does nothing in production code
    io.dispatch(CommLayer::getInstance);
}


unsigned int NetworkManager::sendMessage(const CommAddress & dst, BasicMsg * msg) {
    // Serialize the message
    boost::shared_ptr<Connection> c(new Connection(io));
    std::ostream buffer(&c->writeBuffer);
    msgpack::packer<std::ostream> pk(&buffer);
    // Send the source port number
    uint16_t port = acceptor.local_endpoint().port();
    pk.pack(port);
    msg->pack(pk);
    unsigned int size = c->writeBuffer.size();
    LogMsg("Comm", DEBUG) << "Sending " << *msg << " to " << dst;
    // TODO: Check dst availability
    c->socket.connect(tcp::endpoint(dst.getIP(), dst.getPort()));
    LogMsg("Comm", DEBUG) << "Connection established between src(" << c->socket.local_endpoint() << ") and dst(" << c->socket.remote_endpoint() << ')';
    net::async_write(c->socket, c->writeBuffer.data(), bind(&NetworkManager::handleWrite, this, net::placeholders::error, c));
    return size;
}


void NetworkManager::handleWrite(const boost::system::error_code & error, boost::shared_ptr<Connection> c) {
    // Do nothing, just close connection
}


void NetworkManager::handleAccept(const boost::system::error_code & error) {
    if (!error) {
        // Program an async_read for the data
        LogMsg("Comm", DEBUG) << "Connection accepted between src(" << incoming->socket.remote_endpoint() <<
        ") and dst(" << incoming->socket.local_endpoint() << ')';
        net::streambuf::mutable_buffers_type bufs = incoming->readBuffer.prepare(1500);
        net::async_read(incoming->socket, bufs, net::transfer_at_least(1),
                        bind(&NetworkManager::handleRead, this, net::placeholders::error, net::placeholders::bytes_transferred, incoming));
        // Program a new async_accept
        incoming.reset(new Connection(io));
        acceptor.async_accept(incoming->socket,
                              bind(&NetworkManager::handleAccept, this, net::placeholders::error));
    }
}


void NetworkManager::handleRead(const boost::system::error_code & error, size_t bytes_transferred, boost::shared_ptr<Connection> c) {
    if (!error) {
        // Read until EOF or end of buffer
        c->readBuffer.commit(bytes_transferred);
        net::streambuf::mutable_buffers_type bufs = c->readBuffer.prepare(1500);
        net::async_read(c->socket, bufs, net::transfer_at_least(1),
                        bind(&NetworkManager::handleRead, this, net::placeholders::error, net::placeholders::bytes_transferred, c));
    } else if (error == net::error::eof) {
        // Unserialize the message
        msgpack::unpacker pac;
        try {
            // feeds the buffer.
            size_t bytes = c->readBuffer.size();
            pac.reserve_buffer(bytes);
            c->readBuffer.sgetn(pac.buffer(), bytes);
            pac.buffer_consumed(bytes);
            // Read source port
            uint16_t port;
            msgpack::unpacked msg;
            pac.next(&msg);
            msg.get().convert(&port);
            CommAddress src(c->socket.remote_endpoint().address(), port);
            // Read message
            BasicMsg * bmsg;
            bmsg = BasicMsg::unpackMessage(pac);
            LogMsg("Net", INFO) << "Received message " << *bmsg << " from " << src;
            CommLayer::getInstance().enqueueMessage(src, boost::shared_ptr<BasicMsg>(bmsg));
        } catch (...) {
            LogMsg("Net", ERROR) << "Failed serialization of message from " << c->socket.remote_endpoint();
        }
    }
}


CommAddress NetworkManager::getLocalAddress() const {
    // TODO: pass through NAT, avoid blocking, test different targets, etc...
    // Use sync communication to avoid needing another thread.
    net::io_service tmp_io;
    tcp::socket sock(tmp_io);
    tcp::endpoint google(net::ip::address_v4::from_string("173.194.34.248"), 80);
    sock.connect(google);
    CommAddress result(sock.local_endpoint().address(), ConfigurationManager::getInstance().getPort());
    sock.close();
    return result;
}


void NetworkManager::setAsyncTimer(Time timeout) {
    timer.expires_at(timeout.to_posix_time());
    timer.async_wait(bind(&CommLayer::checkExpired, &CommLayer::getInstance()));
}
