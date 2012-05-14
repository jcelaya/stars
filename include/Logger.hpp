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

#ifndef LOGGER_H_
#define LOGGER_H_

#include <sstream>
#include <list>
#include <boost/serialization/strong_typedef.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/tracking.hpp>
#include "Serializable.hpp"


BOOST_STRONG_TYPEDEF(std::string, tracked_string);
namespace boost {
namespace serialization {
template<class Archive> void serialize(Archive & ar, tracked_string & s, const unsigned int version) {
    ar & s.t;
}
}
}


class LogMsg {
public:
    class AbstractTypeContainer {
        SRLZ_API SRLZ_METHOD() {}
    public:
        virtual ~AbstractTypeContainer() {}
        virtual const AbstractTypeContainer * getSerializableVersion() const = 0;
        virtual void output(std::ostream & os) const = 0;
        friend std::ostream & operator<<(std::ostream & os, const AbstractTypeContainer & r) {
            r.output(os);
            return os;
        }
    };

    class ConstStringReference : public AbstractTypeContainer {
        SRLZ_API SRLZ_METHOD() {
            ar & SERIALIZE_BASE(AbstractTypeContainer) & value;
        }
        tracked_string * value;
        ConstStringReference() : value(NULL) {}

    public:
        ConstStringReference(const char * i) : value(LogMsg::getPersistentString(i)) {}
        const AbstractTypeContainer * getSerializableVersion() const {
            return this;
        };
        void output(std::ostream & os) const {
            os << (std::string)(*value);
        }
    };

    class StringContainer : public AbstractTypeContainer {
        SRLZ_API SRLZ_METHOD() {
            ar & SERIALIZE_BASE(AbstractTypeContainer) & value;
        }
        std::string value;
        StringContainer() {}
    public:
        StringContainer(const std::string & i) : value(i) {}
        const AbstractTypeContainer * getSerializableVersion() const {
            return this;
        }
        void output(std::ostream & os) const {
            os << value;
        }
    };

    template <typename T> class TypeReference : public AbstractTypeContainer {
        SRLZ_API SRLZ_METHOD() {
            ar & SERIALIZE_BASE(AbstractTypeContainer);
            if (IS_LOADING()) {
                T * p = new T();
                ar & *p;
                value = p;
                del = true;
            } else ar & *(const_cast<T *>(value));
        }
        const T * value;
        bool del;
        TypeReference() : value(NULL), del(false) {}
    public:
        TypeReference(const T & i) : value(&i), del(false) {}
        const AbstractTypeContainer * getSerializableVersion() const {
            std::ostringstream oss;
            oss << *value;
            return new StringContainer(oss.str());
        }
        ~TypeReference() {
            if (del) delete value;
        }
        void output(std::ostream & os) const {
            os << *value;
        }
    };

private:
    std::list<AbstractTypeContainer *> values;
    tracked_string * category;
    int priority;

public:
    static void log(tracked_string * category, int priority, const std::list<AbstractTypeContainer *> & values);
    static void setPriority(const std::string & catPrio);
    static tracked_string * getPersistentString(const char * s);

    LogMsg(const char * c, int p) : category(getPersistentString(c)), priority(p) {}
    ~LogMsg() {
        if (!values.empty()) {
            log(category, priority, values);
            while (!values.empty()) {
                delete values.front();
                values.pop_front();
            }
        }
    }

    template <typename T> LogMsg & operator<<(const T & value) {
        values.push_back(new TypeReference<T>(value));
        return *this;
    }

    LogMsg & operator<<(const char * value) {
        values.push_back(new ConstStringReference(value));
        return *this;
    }
};


/**
 * Initializes the logging facility with a configuration string. The string contains the same
 * pairs, separated by a semicolon.
 * @param config String with the configuration of the priorities.
 */
inline void initLog(const std::string & config) {
    int start = 0, end = config.find_first_of(';');
    while (end != (int)std::string::npos) {
        LogMsg::setPriority(config.substr(start, end - start));
        start = end + 1;
        end = config.find_first_of(';', start);
    }
    LogMsg::setPriority(config.substr(start));
}


BOOST_CLASS_TRACKING(LogMsg::AbstractTypeContainer, track_never)
BOOST_CLASS_TRACKING(LogMsg::ConstStringReference, track_never)
BOOST_CLASS_TRACKING(LogMsg::StringContainer, track_never)
// Some TypeContainer instantiations are directly serializable
#define SRLZ_REF(x)\
  template <> inline const LogMsg::AbstractTypeContainer * LogMsg::TypeReference<x>::getSerializableVersion() const {\
   return this;\
  }\
  BOOST_CLASS_TRACKING(LogMsg::TypeReference<x>, track_never)
SRLZ_REF(std::string)
SRLZ_REF(int8_t)
SRLZ_REF(int32_t)
SRLZ_REF(int64_t)
SRLZ_REF(uint8_t)
SRLZ_REF(uint32_t)
SRLZ_REF(uint64_t)
SRLZ_REF(float)
SRLZ_REF(double)

typedef enum {
    EMERG  = 0,
    FATAL  = 0,
    ALERT  = 100,
    CRIT   = 200,
    ERROR  = 300,
    WARN   = 400,
    NOTICE = 500,
    INFO   = 600,
    DEBUG  = 700,
    NOTSET = 800
} log4cppPriorityLevel;



#endif /*LOGGER_H_*/
