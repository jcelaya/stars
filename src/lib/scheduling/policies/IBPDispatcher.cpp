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

#include "Logger.hpp"
#include "IBPDispatcher.hpp"
#include "IBPAvailabilityInformation.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"


/*
 * A block of info associated with a NodeGroup used in the decision algorithm.
 */
struct IBPDispatcher::DecisionInfo {
    IBPAvailabilityInformation::MDCluster & cluster;
    int branch;
    double distance;
    uint64_t availability;

    static const uint32_t ALPHA_MEM = 10;
    static const uint32_t ALPHA_DISK = 1;

    DecisionInfo(IBPAvailabilityInformation::MDCluster & c, uint32_t mem, uint32_t disk, int b, double d)
            : cluster(c), branch(b), distance(d),
              availability((c.getRemainingMemory(mem)) * ALPHA_MEM + (c.getRemainingDisk(disk)) * ALPHA_DISK) {}

    bool operator<(const DecisionInfo & r) {
        return availability < r.availability || (availability == r.availability && distance < r.distance);
    }
};


void IBPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN()) return;
    Logger::msg("Dsp.Simple", INFO, "Received a TaskBagMsg from ", src);
    if (!branch.inNetwork()) {
        Logger::msg("Dsp.Simple", WARN, "TaskBagMsg received but not in network");
        return;
    }
    const TaskDescription & req = msg.getMinRequirements();
    unsigned int remainingTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    Logger::msg("Dsp.Simple", DEBUG, "Requested allocation of ", remainingTasks, " tasks with requirements:");
    Logger::msg("Dsp.Simple", DEBUG, "Memory: ", req.getMaxMemory(), "   Disk: ", req.getMaxDisk());
    Logger::msg("Dsp.Simple", DEBUG, "Length: ", req.getLength(), "   Deadline: ", req.getDeadline());

    // Distribute it downwards

    // First create a list of node groups which can potentially manage the request
    std::list<DecisionInfo> groups;
    // Ignore the zone that has sent this message, only if it is a StructureNode, and zones without information
    for (int c : {0, 1}) {
        if ((msg.isFromEN() || child[c].addr != src) && child[c].availInfo.get()) {
            std::list<IBPAvailabilityInformation::MDCluster *> nodeGroups;
            child[c].availInfo->getAvailability(nodeGroups, req);
            Logger::msg("Dsp.Simple", DEBUG, "Obtained ", nodeGroups.size(), " groups with enough availability from left child.");
            for (std::list<IBPAvailabilityInformation::MDCluster *>::iterator git = nodeGroups.begin(); git != nodeGroups.end(); git++) {
                groups.push_back(DecisionInfo(**git, req.getMaxMemory(), req.getMaxDisk(), c, branch.getChildDistance(c, msg.getRequester())));
            }
        }
    }

    groups.sort();
    Logger::msg("Dsp.Simple", DEBUG, groups.size(), " groups found");

    // Now divide the request between the zones
    std::array<unsigned int, 2> numTasks = {0, 0};
    for (std::list<DecisionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; ++it) {
        Logger::msg("Dsp.Simple", DEBUG, "Using group from ", (it->branch == 0 ? "left" : "right"), " branch and ", it->cluster.getValue(), " nodes, availability is ", it->availability);
        uint32_t numTaken = remainingTasks - it->cluster.takeUpToNodes(remainingTasks);
        numTasks[it->branch] += numTaken;
        remainingTasks -= numTaken;
    }
    for (int c : {0, 1})
        if (numTasks[c]) {
            child[c].availInfo->updated();
            child[c].hasNewInformation = true;
        }
    recomputeInfo();
    notify();

    // Now create and send the messages
    sendTasks(msg, numTasks, branch.getFatherAddress() == src);
}
