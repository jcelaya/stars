/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya, María Ángeles Giménez
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
        Logger::msg("Ex.DescFile", DEBUG, "Executable name: ", executable);

        std::getline(file, result);
        Logger::msg("Ex.DescFile", DEBUG, "Result name: ", result );

        std::getline(file, length);
        Logger::msg("Ex.DescFile", DEBUG, "Task length: ", length );

        std::getline(file, memory);
        Logger::msg("Ex.DescFile", DEBUG, "Memory: ", memory );

        std::getline(file, disk);
        Logger::msg("Ex.DescFile", DEBUG, "Disk: ", disk );
    }
}
