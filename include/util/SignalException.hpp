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

#ifndef SIGNALEXCEPTION_HPP_
#define SIGNALEXCEPTION_HPP_

#include <exception>
#include <string>

class SignalException : public std::exception {
public:
    class Handler {
    public:
        static Handler & getInstance() {
            static Handler instance;
            return instance;
        }

        void setHandler();

        void printStackTrace(std::ostream & oss = std::cerr);
        const char * getStackTraceString();

    private:
        Handler() : stackSize(0), cause(0) {}

        static void handler(int signal);

        static const int stackMaxSize = 25;
        void * stackFunctions[stackMaxSize];
        int stackSize;
        int cause;
        std::string gdbservercmd;
        std::string message;
    };

    virtual const char* what() const throw () {
        //return "Process interrupted by signal.";
        return Handler::getInstance().getStackTraceString();
    }
};

#endif /* SIGNALEXCEPTION_HPP_ */
