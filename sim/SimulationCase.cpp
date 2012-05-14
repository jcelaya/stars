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

#include "SimulationCase.hpp"


boost::shared_ptr<SimulationCase> CaseFactory::createCase(const std::string & name, const Properties & p) {
    std::map < std::string, boost::shared_ptr<SimulationCase> (*)(const Properties & p) >::iterator it = caseConstructors.find(name);
    if (it != caseConstructors.end()) {
        return it->second(p);
    } else return boost::shared_ptr<SimulationCase>();
}
