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

#ifndef BASICMSG_H_
#define BASICMSG_H_

#include <ostream>
#include <iomanip>
#include <msgpack.hpp>

/**
 * \brief Basic message class.
 *
 * This is the base class for any message sent through the network. It is received by the CommLayer object and sent
 * to the handler that registered with the message class.
 */
class BasicMsg {
public:
    template<class Message> class MessageRegistrar {
        static BasicMsg * unpackMessage(const msgpack::object & obj) {
            Message * result = new Message();
            obj.convert(result);
            return result;
        }
    public:
        MessageRegistrar() {
            BasicMsg::unpackerRegistry()[Message::className()] = &unpackMessage;
        }
    };
    
    static BasicMsg * unpackMessage(msgpack::unpacker & pac) {
        std::string name;
        msgpack::unpacked msg;
        pac.next(&msg);
        msg.get().convert(&name);
        pac.next(&msg);
        return unpackerRegistry()[name](msg.get());
    }
    
    virtual ~BasicMsg() {}

    /**
     * Produces an exact copy of this object, regardless of its actual class.
     * @returns A pointer to a base class.
     */
    virtual BasicMsg * clone() const = 0;

    /**
     * Provides a textual representation of this object. Just one line, no endline char.
     * @param os Output stream where to write the representation.
     */
    virtual void output(std::ostream& os) const {}

    /**
     * Provides the name of this message
     */
    virtual std::string getName() const = 0;

    static std::string className() { return std::string("BasicMsg"); }
    
    virtual void pack(msgpack::packer<std::ostream> & buffer) = 0;

private:
    template<class Message> friend class MessageRegistrar;
    static std::map<std::string, BasicMsg * (*)(const msgpack::object &)> & unpackerRegistry() {
        static std::map<std::string, BasicMsg * (*)(const msgpack::object &)> instance;
        return instance;
    }
    
    // Forbid assignment
    BasicMsg & operator=(const BasicMsg &);
};


#define REGISTER_MESSAGE(name) BasicMsg::MessageRegistrar<name> name ## _reg_var
#define MESSAGE_SUBCLASS(name) \
virtual name * clone() const { return new name(*this); } \
virtual std::string getName() const { return className(); } \
virtual void pack(msgpack::packer<std::ostream> & pk) { pk.pack(className()); pk.pack(*this); } \
static std::string className() { return std::string(#name); }


inline std::ostream & operator<<(std::ostream& os, const BasicMsg & s) {
    std::ios_base::fmtflags f = os.flags();
    os << std::setw(0) << s.getName() << ": ";
    s.output(os);
    return os << std::setiosflags(f);
}

#endif /*BASICMSG_H_*/
