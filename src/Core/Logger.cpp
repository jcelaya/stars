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

#include <map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <log4cpp/Category.hh>
#include "Logger.hpp"
using namespace std;
using namespace log4cpp;


// time_duration fix >2 hour digits
struct fix2hourdigits {
	fix2hourdigits() {
		static const char * format = "%O:%M:%S%F";
		boost::posix_time::time_facet::default_time_duration_format = format;
	}
} fix2hourdigits_var;


tracked_string * LogMsg::getPersistentString(const char * s) {
	static map<const char *, tracked_string> stringMap;

	tracked_string & result = stringMap[s];
	if (result.t.size() == 0)
		result.t = s;
	return &result;
}


void LogMsg::setPriority(const string & catPrio) {
	int pos = catPrio.find_first_of('=');
	if (pos == (int)string::npos) return;
	string category = catPrio.substr(0, pos);
	string priority = catPrio.substr(pos + 1);
	try {
		Priority::Value p = Priority::getPriorityValue(priority);
		if (category == "root") {
			Category::setRootPriority(p);
		} else {
			Category::getInstance(category).setPriority(p);
		}
	} catch (std::invalid_argument & e) {}
}


void LogMsg::log(tracked_string * category, int priority, const list<LogMsg::AbstractTypeContainer *> & values) {
	Category & cat = Category::getInstance(*category);
	if (cat.isPriorityEnabled(priority)) {
		CategoryStream cs = cat.getStream(priority);
		for (list<LogMsg::AbstractTypeContainer *>::const_iterator it = values.begin(); it != values.end(); it++)
			cs << **it;
	}
}
