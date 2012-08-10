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
//        if (boost::dynamic_pointer_cast<AvailabilityInformation>(ev.msg)->isFromSch()) {
//            if (child.getE().getFather() == CommAddress() || ev.to != child.getE().getFather().getIPNum()) return;
//        } else {
//            if (child.getS().getFather() == CommAddress() || ev.to != child.getS().getFather().getIPNum()) return;
//        }
//    }

    Simulator & sim = Simulator::getInstance();
    StarsNode & node = sim.getNode(dst);
    // If the change at the origin is not valid, discard it.
    if (!node.getS().isRNChildren() && !activeChanges[src].valid) {
        LogMsg("Sim.Stat.Avail", DEBUG) << src << " -> " << dst << ": " << "Old information, skipping";
        return;
    }

    // If there was a valid change in the current node, it ends here.
    Change & thisChange = activeChanges[dst];
    if (thisChange.valid) {
        double t = thisChange.duration();
        int l = node.getS().getLevel();
        updateTimes.addValue(t);
        reachedLevel.addValue(l);
        os << t << ',' << l << std::endl;
        LogMsg("Sim.Stat.Avail", DEBUG) << src << " -> " << dst << ": " << "A change from " << t << " seconds ago at level " << l;
    }

    // Create new change
    thisChange.valid = true;
    thisChange.end = sim.getCurrentTime();
    if (node.getS().isRNChildren()) {
        thisChange.creation = c;
    } else {
        thisChange.creation = activeChanges[dst].creation;
        // Invalidate last change
        activeChanges[dst].valid = false;
    }
}


void AvailabilityStatistics::finishAvailabilityStatistics() {
    // Write all valid changes
    Simulator & sim = Simulator::getInstance();
    for (unsigned int i = 0; i < activeChanges.size(); ++i) {
        if (activeChanges[i].valid) {
            double t = activeChanges[i].duration();
            int l = sim.getNode(i).getS().getLevel();
            updateTimes.addValue(t);
            reachedLevel.addValue(l);
            os << t << ',' << l << std::endl;
        }
    }

    // End this index
    os << std::endl << std::endl;
    os << "# Update time CDF" << std::endl << CDF(updateTimes);
    // End this index
    os << std::endl << std::endl;
    os << "# Reached level CDF" << std::endl << CDF(reachedLevel) << std::endl << std::endl;
    os.close();
}
