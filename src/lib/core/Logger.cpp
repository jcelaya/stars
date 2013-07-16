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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <log4cpp/Category.hh>
#include "Logger.hpp"


namespace {
// time_duration fix >2 hour digits
struct fix2hourdigits {
    fix2hourdigits() {
        static const char * format = "%O:%M:%S%F";
        boost::posix_time::time_facet::default_time_duration_format = format;
    }
} fix2hourdigits_var;
}


std::string LogMsg::Indent::currentIndent;
bool LogMsg::Indent::active = true;
const LogMsg::Indent LogMsg::indent;

void LogMsg::setPriority(const std::string & catPrio) {
    int pos = catPrio.find_first_of('=');
    if (pos == (int)std::string::npos) return;
    std::string category = catPrio.substr(0, pos);
    std::string priority = catPrio.substr(pos + 1);
    try {
        log4cpp::Priority::Value p = log4cpp::Priority::getPriorityValue(priority);
        if (category == "root") {
            log4cpp::Category::setRootPriority(p);
        } else {
            log4cpp::Category::getInstance(category).setPriority(p);
        }
    } catch (std::invalid_argument & e) {}
}


void LogMsg::log(const char * category, int priority, LogMsg::AbstractTypeContainer * values) {
    log4cpp::Category & cat = log4cpp::Category::getInstance(category);
    if (cat.isPriorityEnabled(priority)) {
        log4cpp::CategoryStream cs = cat.getStream(priority);
        for (AbstractTypeContainer * it = values; it != NULL; it = it->next)
            cs << *it;
    }
}
