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

#include <msgpack.hpp>
#include "NetworkManager.hpp"
#include "CommLayer.hpp"
#include "ConfigurationManager.hpp"
#include "Logger.hpp"
namespace net = boost::asio;
using boost::bind;
using net::ip::tcp;


NetworkManager::Connection::~Connection() {
    boost::system::error_code ec;
    socket.shutdown(tcp::socket::shutdown_both, ec);
    socket.close(ec);
}


NetworkManager::NetworkManager() : acceptor(io), incoming(new Connection(io)), timer(io) {}


void NetworkManager::listen() {
    // TODO: error checking
    boost::system::error_code ec;
    // Start async accept before creating the thread. It will maintain the thread alive.
    acceptor.open(tcp::v4(), ec);
    acceptor.bind(tcp::endpoint(tcp::v4(), ConfigurationManager::getInstance().getPort()), ec);
    acceptor.listen(net::socket_base::max_connections, ec);
    acceptor.async_accept(incoming->socket,
                          bind(&NetworkManager::handleAccept, this, net::placeholders::error));
    t.reset(new boost::thread(bind((size_t (net::io_service:: *)())(&net::io_service::run), &io)));
    Logger::msg("Net", INFO, "Thread ", t->get_id(), " accepting connections on port ", ConfigurationManager::getInstance().getPort());
    // The following is needed for the test cases, it does nothing in production code
    io.dispatch(CommLayer::getInstance);
}


unsigned int NetworkManager::sendMessage(const CommAddress & dst, BasicMsg * msg) {
    // Serialize the message
    std::shared_ptr<Connection> c(new Connection(io));
    c->dst = dst;
    std::ostream buffer(&c->writeBuffer);
    msgpack::packer<std::ostream> pk(&buffer);
    // Send the source port number
    uint16_t port = acceptor.local_endpoint().port();
    pk.pack(port);
    msg->pack(pk);
    unsigned int size = c->writeBuffer.size();
    Logger::msg("Comm", DEBUG, "Sending ", *msg, " to ", dst);
    c->socket.async_connect(tcp::endpoint(dst.getIP(), dst.getPort()),
            bind(&NetworkManager::handleConnect, this, net::placeholders::error, c));
    return size;
}


void NetworkManager::handleConnect(const boost::system::error_code & error, std::shared_ptr<Connection> c) {
    if (!error) {
        Logger::msg("Comm", DEBUG, "Connection established with ", c->dst);
        net::async_write(c->socket, c->writeBuffer.data(),
                bind(&NetworkManager::handleWrite, this, net::placeholders::error, c));
    } else {
        Logger::msg("Comm", WARN, "Destination unreachable: ", c->dst);
    }
}


void NetworkManager::handleWrite(const boost::system::error_code & error, std::shared_ptr<Connection> c) {
    // Do nothing, just close connection
}


void NetworkManager::handleAccept(const boost::system::error_code & error) {
    if (!error) {
        // Program an async_read for the data
        Logger::msg("Comm", DEBUG, "Connection accepted between src(", incoming->socket.remote_endpoint(),
                ") and dst(", incoming->socket.local_endpoint(), ')');
        net::streambuf::mutable_buffers_type bufs = incoming->readBuffer.prepare(1500);
        net::async_read(incoming->socket, bufs, net::transfer_at_least(1),
                        bind(&NetworkManager::handleRead, this, net::placeholders::error, net::placeholders::bytes_transferred, incoming));
        // Program a new async_accept
        incoming.reset(new Connection(io));
        acceptor.async_accept(incoming->socket,
                              bind(&NetworkManager::handleAccept, this, net::placeholders::error));
    }
}


void NetworkManager::handleRead(const boost::system::error_code & error, size_t bytes_transferred, std::shared_ptr<Connection> c) {
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
            Logger::msg("Net", INFO, "Received message ", *bmsg, " from ", src);
            CommLayer::getInstance().enqueueMessage(src, std::shared_ptr<BasicMsg>(bmsg));
        } catch (...) {
            Logger::msg("Net", ERROR, "Failed serialization of message from ", c->socket.remote_endpoint());
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
