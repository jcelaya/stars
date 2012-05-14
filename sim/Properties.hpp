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

#ifndef PROPERTIES_H_
#define PROPERTIES_H_

#include <map>
#include <string>
#include <sstream>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;


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

    void loadFromFile(const fs::path & fileName);

    friend std::ostream & operator<<(std::ostream & os, const Properties & o);
};

#endif /*PROPERTIES_H_*/
