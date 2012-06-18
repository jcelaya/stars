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

#ifndef COMMADDRESS_H_
#define COMMADDRESS_H_

#include <string>
#include <iomanip>
#include <ostream>
#include <boost/asio.hpp>
#include <msgpack.hpp>


/**
 * \brief Address class.
 *
 * This class represents a Peer address. It is a pair IP address - TCP/UDP port.
 **/
class CommAddress {
public:
    CommAddress() : IP(boost::asio::ip::address_v4(0)), port(0) {}

    /// Constructor from IP and port values in integer form.
    CommAddress(const boost::asio::ip::address & IPin, uint16_t portIn)
            : IP(IPin), port(portIn) {}

    /// Constructor from IP and port values in integer form.
    CommAddress(uint32_t IPin, uint16_t portIn)
            : IP(boost::asio::ip::address_v4(IPin)), port(portIn) {}

    /// Constructor from a IP address in dotted form and a port.
    CommAddress(std::string IPin, uint16_t portIn)
            : IP(boost::asio::ip::address::from_string(IPin)), port(portIn) {}

    /**
     * Calculates the value of this address, to compare two addresses.
     * @return A simple value calculated from this address.
     */
    double value() const {
        return (double)IP.to_v4().to_ulong() + (port / 65536.0);
    }

    /**
     * Equality operator.
     * @param r The right operand.
     * @return True if both objects are equal.
     */
    bool operator==(const CommAddress & r) const {
        return IP == r.IP && port == r.port;
    }

    /**
     * Inequality operator.
     * @param r The right operand.
     * @return False if both objects are equal.
     */
    bool operator!=(const CommAddress & r) const {
        return IP != r.IP || port != r.port;
    }

    /**
     * Less operator.
     * @param r The right operand.
     * @return True if the value of this object is lower than the value of r.
     */
    bool operator<(const CommAddress & r) const {
        return value() < r.value();
    }

    /**
     * Less or equal operator.
     * @param r The right operand.
     * @return True if the value of this object is lower than or equal to the value of r.
     */
    bool operator<=(const CommAddress & r) const {
        return value() <= r.value();
    }

    /**
     * Distance between two network addresses.
     * @param r The right operand.
     * @return The distance between two network addresses. It is a real value
     *         greater than 0.0 independently of the order of the operands.
     */
    double distance(const CommAddress & r) const {
        double result = value() - r.value();
        return (result < 0.0 ? -result : result);
    }

    /**
     * Returns the IP address in dotted form.
     * @return A string containing the result.
     */
    std::string getIPString() const {
        return IP.to_string();
    }

    /**
     * Obtains the IP address in integer form.
     * @return IP address.
     */
    const boost::asio::ip::address & getIP() const {
        return IP;
    }

    uint32_t getIPNum() const {
        return IP.to_v4().to_ulong();
    }

    /**
     * Obtains the port value as an integer.
     */
    uint16_t getPort() const {
        return port;
    }

    /**
     * Writes a representation of this CommAddress object in an ostream object.
     * @param os Output stream where to write this object.
     */
    friend std::ostream & operator<<(std::ostream& os, const CommAddress & s) {
        std::ios_base::fmtflags f = os.flags();
        return os << std::setw(0) << s.getIPString() << ":" << s.port << std::setiosflags(f);
    }
    
    template <typename Packer> void msgpack_pack(Packer & pk) const {
        pk.pack_array(2);
        std::string ipstr = IP.to_string();
        pk.pack(ipstr);
        pk.pack(port);
    }
    void msgpack_unpack(msgpack::object o) {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        std::string ipstr;
        o.via.array.ptr[0].convert(&ipstr);
        o.via.array.ptr[1].convert(&port);
        IP = boost::asio::ip::address::from_string(ipstr);
    }
private:
    boost::asio::ip::address IP;   ///< IP address
    uint16_t port;                 ///< TCP/UDP port
};

#endif /*COMMADDRESS_H_*/
