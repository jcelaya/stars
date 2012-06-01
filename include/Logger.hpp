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

#include <string>
#include <ostream>


class LogMsg {
public:
    class AbstractTypeContainer {
    public:
        AbstractTypeContainer() : next(NULL) {}
        virtual ~AbstractTypeContainer() {}
        virtual void output(std::ostream & os) const = 0;
        friend std::ostream & operator<<(std::ostream & os, const AbstractTypeContainer & r) {
            r.output(os);
            return os;
        }
        
        AbstractTypeContainer * next;
    };
    
    /**
     * Initializes the logging facility with a configuration string. The string contains the same
     * pairs, separated by a semicolon.
     * @param config String with the configuration of the priorities.
     */
    static void initLog(const std::string & config) {
        int start = 0, end = config.find_first_of(';');
        while (end != (int)std::string::npos) {
            setPriority(config.substr(start, end - start));
            start = end + 1;
            end = config.find_first_of(';', start);
        }
        setPriority(config.substr(start));
    }
    
    LogMsg(const char * c, int p) : first(NULL), last(NULL), category(c), priority(p) {}
    
    ~LogMsg() {
        if (first != NULL) {
            log(category, priority, first);
            while (first != NULL) {
                AbstractTypeContainer * next = first->next;
                delete first;
                first = next;
            }
        }
    }
    
    template <typename T> LogMsg & operator<<(const T & value) {
        AbstractTypeContainer * n = new TypeReference<T>(value);
        if (first == NULL)
            first = last = n;
        else
            last = last->next = n;
        return *this;
    }

private:
    template <typename T> class TypeReference : public AbstractTypeContainer {
        const T * value;
    public:
        TypeReference(const T & i) : value(&i) {}
        TypeReference(const T * i) : value(i) {}
        void output(std::ostream & os) const {
            os << *value;
        }
    };
    
    AbstractTypeContainer * first, * last;
    const char * category;
    int priority;
    static void log(const char * category, int priority, AbstractTypeContainer * values);
    static void setPriority(const std::string & catPrio);
};


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
