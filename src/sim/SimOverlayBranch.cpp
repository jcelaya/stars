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

#include "SimOverlayBranch.hpp"
#include "StarsNode.hpp"
#include "Simulator.hpp"


void SimOverlayBranch::build(const CommAddress & l, bool lb, const CommAddress & r, bool rb) {
    Simulator & sim = Simulator::getInstance();
    left = l;
    right = r;
    StarsNode & leftNode = sim.getNode(l.getIPNum());
    if (lb)
        leftZone = static_cast<SimOverlayBranch &>(leftNode.getBranch()).getZone();
    else
        leftZone = ZoneDescription(l);
    StarsNode & rightNode = sim.getNode(r.getIPNum());
    if (rb)
        rightZone = static_cast<SimOverlayBranch &>(rightNode.getBranch()).getZone();
    else
        rightZone = ZoneDescription(r);
}

