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

#include <execinfo.h>
#include <cxxabi.h>
#include <signal.h>
#include <unistd.h>
#include <malloc.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include "util/SignalException.hpp"


void SignalException::Handler::setHandler() {
    std::ostringstream oss;
    // FIXME: only one debugged process at a time...
    //oss << "/usr/bin/gdbserver host:12345 --attach " << getpid();
    oss << "/usr/local/bin/gdbserverwrapper " << getpid();
    gdbservercmd = oss.str();
    signal(SIGSEGV, SignalException::Handler::handler);
    signal(SIGILL, SignalException::Handler::handler);
    signal(SIGFPE, SignalException::Handler::handler);
    signal(SIGBUS, SignalException::Handler::handler);
}


void SignalException::Handler::handler(int signal) {
    SignalException::Handler & h = getInstance();
    h.cause = signal;
    h.stackSize = backtrace(h.stackFunctions, stackMaxSize);
#ifndef NDEBUG
    std::cerr << "Signal trapped, launching gdbserver" << std::endl;
    system(h.gdbservercmd.c_str());
#endif
    throw SignalException();
}


void SignalException::Handler::printStackTrace() {
    char ** symbols = backtrace_symbols(stackFunctions, stackSize);

    switch (cause) {
    case SIGSEGV: std::cerr << "Segmentation fault"; break;
    default: std::cerr << "Caught signal " << cause; break;
    }
    std::cerr << ", backtrace: " << std::endl;
    for (int i = 0; i < stackSize; ++i) {
        size_t sz = 200; // just a guess, template names will go much wider
        char *function = static_cast<char *>(malloc(sz));
        char *begin = 0, *end = 0;
        // find the parentheses and address offset surrounding the mangled name
        for (char *j = symbols[i]; *j; ++j) {
            if (*j == '(') begin = j;
            else if (*j == '+') end = j;
        }
        if (begin && end) {
            *begin++ = *end = '\0';
            // found our mangled name, now in [begin, end)

            int status;
            char *ret = abi::__cxa_demangle(begin, function, &sz, &status);
            if (ret) {
                // return value may be a realloc() of the input
                function = ret;
            } else {
                // demangling failed, just pretend it's a C function with no args
                std::strncpy(function, begin, sz);
                std::strncat(function, "()", sz);
                function[sz-1] = '\0';
            }
            std::cerr << "    " << symbols[i] << ':' << function << std::endl;
        } else {
            // didn't find the mangled name, just print the whole line
            std::cerr << "    " << symbols[i] << std::endl;
        }
        free(function);
    }

    free(symbols);
}


const char * SignalException::Handler::getStackTraceString() {
    char ** symbols = backtrace_symbols(stackFunctions, stackSize);
    std::ostringstream oss;

    switch (cause) {
    case SIGSEGV: oss << "Segmentation fault"; break;
    default: oss << "Caught signal " << cause; break;
    }
    oss << ", backtrace: " << std::endl;
    for (int i = 0; i < stackSize; ++i) {
        size_t sz = 200; // just a guess, template names will go much wider
        char *function = static_cast<char *>(malloc(sz));
        char *begin = 0, *end = 0;
        // find the parentheses and address offset surrounding the mangled name
        for (char *j = symbols[i]; *j; ++j) {
            if (*j == '(') begin = j;
            else if (*j == '+') end = j;
        }
        if (begin && end) {
            *begin++ = *end = '\0';
            // found our mangled name, now in [begin, end)

            int status;
            char *ret = abi::__cxa_demangle(begin, function, &sz, &status);
            if (ret) {
                // return value may be a realloc() of the input
                function = ret;
            } else {
                // demangling failed, just pretend it's a C function with no args
                std::strncpy(function, begin, sz);
                std::strncat(function, "()", sz);
                function[sz-1] = '\0';
            }
            oss << "    " << symbols[i] << ':' << function << std::endl;
        } else {
            // didn't find the mangled name, just print the whole line
            oss << "    " << symbols[i] << std::endl;
        }
        free(function);
    }

    free(symbols);
    message = oss.str();
    return message.c_str();
}
