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

#ifndef MSGPACKARCHIVE_HPP_
#define MSGPACKARCHIVE_HPP_

#include "TransactionalZoneDescription.hpp"


class MsgpackOutArchive {
    msgpack::packer<msgpack::sbuffer> & pk;
    static const bool valid, invalid;
public:
    MsgpackOutArchive(msgpack::packer<msgpack::sbuffer> & o) : pk(o) {}
    template<class T> MsgpackOutArchive & operator&(T & o) {
        pk.pack(o);
        return *this;
    }
    template<class T> MsgpackOutArchive & operator&(std::shared_ptr<T> & o) {
        if (o.get()) {
            pk.pack(valid);
            *this & *o;
        } else {
            pk.pack(invalid);
        }
        return *this;
    }
    template<class T> MsgpackOutArchive & operator&(std::list<T> & o) {
        size_t size = o.size();
        pk.pack(size);
        for (typename std::list<T>::iterator i = o.begin(); i != o.end(); ++i)
            *this & *i;
        return *this;
    }
};

template<> inline MsgpackOutArchive & MsgpackOutArchive::operator&(TransactionalZoneDescription & o) {
    o.serializeState(*this);
    return *this;
}


class MsgpackInArchive {
    msgpack::unpacker & upk;
    msgpack::unpacked msg;
public:
    MsgpackInArchive(msgpack::unpacker & o) : upk(o) {}
    template<class T> MsgpackInArchive & operator&(T & o) {
        upk.next(&msg); msg.get().convert(&o);
        return *this;
    }
    template<class T> MsgpackInArchive & operator&(std::shared_ptr<T> & o) {
        bool valid;
        upk.next(&msg); msg.get().convert(&valid);
        if (valid) {
            o.reset(new T());
            *this & *o;
        } else o.reset();
        return *this;
    }
    template<class T> MsgpackInArchive & operator&(std::list<T> & o) {
        size_t size;
        upk.next(&msg); msg.get().convert(&size);
        for (size_t i = 0; i < size; ++i) {
            o.push_back(T());
            *this & o.back();
        }
        return *this;
    }
};

template<> inline MsgpackInArchive & MsgpackInArchive::operator&(TransactionalZoneDescription & o) {
    o.serializeState(*this);
    return *this;
}


#endif /* MSGPACKARCHIVE_HPP_ */
