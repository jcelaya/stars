/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2008 Javier Celaya
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

#include <boost/filesystem/fstream.hpp>
#include "Logger.hpp"
#include "DescriptionFile.hpp"
#include "Logger.hpp"
#include "ConfigurationManager.hpp"
using namespace std;
using namespace boost;


DescriptionFile::DescriptionFile(string taskName) {
    filesystem::path fileName(ConfigurationManager::getInstance().getWorkingPath() / taskName  / "description.conf");
    filesystem::ifstream file(fileName);

    if (file) {
        std::getline(file, executable);
        LogMsg("Ex.DescFile", DEBUG) << "Executable name: " << executable;

        std::getline(file, result);
        LogMsg("Ex.DescFile", DEBUG) << "Result name: " << result ;

        std::getline(file, length);
        LogMsg("Ex.DescFile", DEBUG) << "Task length: " << length ;

        std::getline(file, memory);
        LogMsg("Ex.DescFile", DEBUG) << "Memory: " << memory ;

        std::getline(file, disk);
        LogMsg("Ex.DescFile", DEBUG) << "Disk: " << disk ;
    }
}
