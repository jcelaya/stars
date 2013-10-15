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

    static const class Indent {
    public:
        friend std::ostream & operator<<(std::ostream & os, const Indent & r) {
            if (active)
                os << std::endl << currentIndent;
            return os;
        }
    private:
        friend class LogMsg;
        static std::string currentIndent;
        static bool active;
    } indent;

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

    static void setIndent(size_t n) {
        Indent::currentIndent = std::string(n, ' ');
    }

    static void setIndentActive(bool active) {
        Indent::active = active;
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

    template <typename... Args>
    static void logMsg(const char * category, int priority, const Args &... params) {
        std::ostream * out = streamIfEnabled(category, priority);
        if (out) {
            output(*out, params...);
            *out << std::endl;
            freeStream(out);
        }
    }

private:
    static void output(std::ostream & out) {}

    template<typename T, typename... Args>
    static void output(std::ostream & out, const T & value, const Args &... args) {
        out << value;
        output(out, args...);
    }

    static std::ostream * streamIfEnabled(const char * category, int priority);
    static void freeStream(std::ostream * out);

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
