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
#include "DPDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using namespace std;


const Duration DPDispatcher::REQUEST_CACHE_TIME(10.0);
const unsigned int DPDispatcher::REQUEST_CACHE_SIZE = 100;


/*
 * A block of info associated with a NodeGroup used in the decission algorithm.
 */
struct DPDispatcher::DecissionInfo {
    DPAvailabilityInformation::AssignmentInfo ai;
    int branch;
    double distance;
    uint64_t availability;

    static const uint32_t ALPHA_MEM = 10;
    static const uint32_t ALPHA_DISK = 1;
    static const uint32_t ALPHA_COMP = 100;

    DecissionInfo(const DPAvailabilityInformation::AssignmentInfo & c, int b, double d)
            : ai(c), branch(b), distance(d) {
        availability = ALPHA_MEM * ai.remngMem + ALPHA_DISK * ai.remngDisk + ALPHA_COMP * ai.remngAvail;
    }

    bool operator<(const DecissionInfo & r) {
        return availability < r.availability || (availability == r.availability && distance < r.distance);
    }
};


void DPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN()) return;
    Logger::msg("Dsp.Dl", INFO, "Received a TaskBagMsg from ", src);
    if (!branch.inNetwork()) {
        Logger::msg("Dsp.Dl", WARN, "TaskBagMsg received but not in network");
        return;
    }

    // Check if we already routed this request recently
    Time now = Time::getCurrentTime();
    {
        list<RecentRequest>::iterator it = recentRequests.begin();
        while (it != recentRequests.end() && now - it->when > REQUEST_CACHE_TIME)
            it = recentRequests.erase(it);
        while (it != recentRequests.end() && !(it->requestId == msg.getRequestId() && it->requester == msg.getRequester()))
            it++;
        if (it != recentRequests.end()) {
            recentRequests.push_back(RecentRequest(msg.getRequester(), msg.getRequestId(), now));
            recentRequests.erase(it);
            if (father.addr != CommAddress()) {
                CommLayer::getInstance().sendMessage(father.addr, msg.clone());
            }
            return;
        }
    }
    // The request wasn't in the cache
    recentRequests.push_back(RecentRequest(msg.getRequester(), msg.getRequestId(), now));
    if (recentRequests.size() > REQUEST_CACHE_SIZE) recentRequests.pop_front();

    const TaskDescription & req = msg.getMinRequirements();
    unsigned int remainingTasks = msg.getLastTask() - msg.getFirstTask() + 1;
    unsigned int nextTask = msg.getFirstTask();
    Logger::msg("Dsp.Dl", INFO, "Requested allocation of ", remainingTasks, " tasks with requirements:");
    Logger::msg("Dsp.Dl", INFO, "Memory: ", req.getMaxMemory(), "   Disk: ", req.getMaxDisk());
    Logger::msg("Dsp.Dl", INFO, "Length: ", req.getLength(), "   Deadline: ", req.getDeadline());

    // Distribute it downwards

    // First create a list of node groups which can potentially manage the request
    list<DecissionInfo> groups;
    // Ignore the zone that has sent this message, only if it is a StructureNode, and zones without information
    for (int c : {0, 1}) {
        if ((msg.isFromEN() || child[c].addr != src) && child[c].availInfo.get()) {
            list<DPAvailabilityInformation::AssignmentInfo> ai;
            child[c].availInfo->getAvailability(ai, req);
            Logger::msg("Dsp.Dl", DEBUG, "Obtained ", ai.size(), " groups with enough availability");
            for (list<DPAvailabilityInformation::AssignmentInfo>::iterator git = ai.begin();
                    git != ai.end(); git++) {
                Logger::msg("Dsp.Dl", DEBUG, git->numTasks, " tasks with remaining availability ", git->remngAvail);
                groups.push_back(DecissionInfo(*git, c, branch.getChildDistance(c, msg.getRequester())));
            }
        }
    }

    groups.sort();
    Logger::msg("Dsp.Dl", DEBUG, groups.size(), " groups found");

    // Now divide the request between the zones
    list<DPAvailabilityInformation::AssignmentInfo> assign[2];
    std::array<unsigned int, 2> numTasks = {0, 0};
    for (list<DecissionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; it++) {
        Logger::msg("Dsp.Dl", DEBUG, "Using group from ", (it->branch == 0 ? "left" : "right"), " branch and ", it->ai.numTasks, " tasks");
        unsigned int tasksInGroup = it->ai.numTasks;
        if (remainingTasks > tasksInGroup) {
            numTasks[it->branch] += tasksInGroup;
            remainingTasks -= tasksInGroup;
            assign[it->branch].push_back(it->ai);
        } else {
            numTasks[it->branch] += remainingTasks;
            assign[it->branch].push_back(it->ai);
            assign[it->branch].back().numTasks = remainingTasks;
            remainingTasks = 0;
        }
    }

    // Update the availability information
    for (int c : {0, 1})
        if (child[c].availInfo.get())
            child[c].availInfo->update(assign[c], req);
    // Send the result to the father
    recomputeInfo();
    notify();

    sendTasks(msg, numTasks, father.addr == CommAddress());
}
