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

#include "AvailabilityStatistics.hpp"
#include "StructureNode.hpp"
#include "Logger.hpp"
#include "StarsNode.hpp"
#include "StructureNode.hpp"
#include "ResourceNode.hpp"
#include "Simulator.hpp"


void AvailabilityStatistics::openStatsFiles(const boost::filesystem::path & statDir) {
    os.open(statDir / fs::path("availability.stat"));
    os << "# Update time, reached level" << std::endl;
}


void AvailabilityStatistics::changeUpwards(uint32_t src, uint32_t dst, Time c) {
//    {
//        // Check that ev.to is the father of ev.from. Only count changes upwards.
//        StarsNode & child = sim.getNode(ev.from);
//        if (std::dynamic_pointer_cast<AvailabilityInformation>(ev.msg)->isFromSch()) {
//            if (child.getE().getFather() == CommAddress() || ev.to != child.getE().getFather().getIPNum()) return;
//        } else {
//            if (child.getS().getFather() == CommAddress() || ev.to != child.getS().getFather().getIPNum()) return;
//        }
//    }

//    Simulator & sim = Simulator::getInstance();
//    StarsNode & node = sim.getNode(dst);
//    // If the change at the origin is not valid, discard it.
//    if (!node.getS().isRNChildren() && !activeChanges[src].valid) {
//        LogMsg("Sim.Stat.Avail", DEBUG) << src << " -> " << dst << ": " << "Old information, skipping";
//        return;
//    }
//
//    // If there was a valid change in the current node, it ends here.
//    Change & thisChange = activeChanges[dst];
//    if (thisChange.valid) {
//        double t = thisChange.duration();
//        int l = node.getS().getLevel();
//        os << t << ',' << l << std::endl;
//        LogMsg("Sim.Stat.Avail", DEBUG) << src << " -> " << dst << ": " << "A change from " << t << " seconds ago at level " << l;
//    }
//
//    // Create new change
//    thisChange.valid = true;
//    thisChange.end = sim.getCurrentTime();
//    if (node.getS().isRNChildren()) {
//        thisChange.creation = c;
//    } else {
//        thisChange.creation = activeChanges[dst].creation;
//        // Invalidate last change
//        activeChanges[dst].valid = false;
//    }
}


void AvailabilityStatistics::finishAvailabilityStatistics() {
    // Write all valid changes
    Simulator & sim = Simulator::getInstance();
    for (unsigned int i = 0; i < activeChanges.size(); ++i) {
        if (activeChanges[i].valid) {
            double t = activeChanges[i].duration();
            int l = sim.getNode(i).getBranchLevel();
            os << t << ',' << l << std::endl;
        }
    }
    os.close();
}
