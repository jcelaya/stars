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

#include "SimulationCase.hpp"
#include "Simulator.hpp"

void SimulationCase::finishedApp(long int appId) {
    Simulator::getInstance().getCurrentNode().getDatabase().appInstanceFinished(appId);
}


boost::shared_ptr<SimulationCase> CaseFactory::createCase(const std::string & name, const Properties & p) {
    std::map<std::string, boost::shared_ptr<SimulationCase> (*)(const Properties & p)>::iterator it = caseConstructors.find(name);
    if (it != caseConstructors.end()) {
        return it->second(p);
    } else return boost::shared_ptr<SimulationCase>();
}
