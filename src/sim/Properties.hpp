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

#ifndef PROPERTIES_H_
#define PROPERTIES_H_

#include <map>
#include <string>
#include <sstream>


class Properties : public std::map<std::string, std::string> {
public:
    template <class T>
    const T operator()(const std::string & key, const T & defaultValue) const {
        const_iterator i = find(key);
        if (i != end()) {
            T t = defaultValue;
            std::istringstream iss(i->second);
            iss >> t;
            return t;
        } else return defaultValue;
    }

    void loadFrom(std::istream & is);

    friend std::ostream & operator<<(std::ostream & os, const Properties & o);
};

#endif /*PROPERTIES_H_*/
