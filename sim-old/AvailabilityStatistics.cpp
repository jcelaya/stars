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
#include "PeerCompNode.hpp"
#include "StructureNode.hpp"
#include "ResourceNode.hpp"


AvailabilityStatistics::AvailabilityStatistics() : Simulator::InterEventHandler(), updateTimes(0.01), reachedLevel(1.0) {
    activeChanges.resize(sim.getNumNodes());
    os.open(sim.getResultDir() / fs::path("availability.stat"));
    os << "# Update time, reached level" << std::endl;
}


void AvailabilityStatistics::afterEvent(const Simulator::Event & ev) {
    if (boost::dynamic_pointer_cast<AvailabilityInformation>(ev.msg).get()) {
        {
            // Check that ev.to is the father of ev.from. Only count changes upwards.
            PeerCompNode & child = sim.getNode(ev.from);
            if (boost::dynamic_pointer_cast<AvailabilityInformation>(ev.msg)->isFromSch()) {
                if (child.getE().getFather() == CommAddress() || ev.to != child.getE().getFather().getIPNum()) return;
            } else {
                if (child.getS().getFather() == CommAddress() || ev.to != child.getS().getFather().getIPNum()) return;
            }
        }

        PeerCompNode & node = sim.getNode(ev.to);
        // If the change at the origin is not valid, discard it.
        if (!node.getS().isRNChildren() && !activeChanges[ev.from].valid) {
            LogMsg("Sim.Stat.Avail", DEBUG) << ev.from << " -> " << ev.to << ": " << "Old information, skipping";
            return;
        }

        // If there was a valid change in the current node, it ends here.
        Change & thisChange = activeChanges[ev.to];
        if (thisChange.valid) {
            double t = thisChange.duration();
            int l = node.getS().getLevel();
            updateTimes.addValue(t);
            reachedLevel.addValue(l);
            os << t << ',' << l << std::endl;
            LogMsg("Sim.Stat.Avail", DEBUG) << ev.from << " -> " << ev.to << ": " << "A change from " << t << " seconds ago at level " << l;
        }

        // Create new change
        thisChange.valid = true;
        thisChange.end = ev.t;
        if (node.getS().isRNChildren()) {
            thisChange.creation = ev.creationTime;
        } else {
            thisChange.creation = activeChanges[ev.from].creation;
            // Invalidate last change
            activeChanges[ev.from].valid = false;
        }
    }
}


AvailabilityStatistics::~AvailabilityStatistics() {
    // Write all valid changes
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
