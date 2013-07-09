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
    bool leftBranch;
    double distance;
    uint64_t availability;

    static const uint32_t ALPHA_MEM = 10;
    static const uint32_t ALPHA_DISK = 1;
    static const uint32_t ALPHA_COMP = 100;

    DecissionInfo(const DPAvailabilityInformation::AssignmentInfo & c, bool b, double d)
            : ai(c), leftBranch(b), distance(d) {
        availability = ALPHA_MEM * ai.remngMem + ALPHA_DISK * ai.remngDisk + ALPHA_COMP * ai.remngAvail;
    }

    bool operator<(const DecissionInfo & r) {
        return availability < r.availability || (availability == r.availability && distance < r.distance);
    }
};


void DPDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
    if (msg.isForEN()) return;
    LogMsg("Dsp.Dl", INFO) << "Received a TaskBagMsg from " << src;
    if (!branch.inNetwork()) {
        LogMsg("Dsp.Dl", WARN) << "TaskBagMsg received but not in network";
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
    LogMsg("Dsp.Dl", INFO) << "Requested allocation of " << remainingTasks << " tasks with requirements:";
    LogMsg("Dsp.Dl", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk();
    LogMsg("Dsp.Dl", INFO) << "Length: " << req.getLength() << "   Deadline: " << req.getDeadline();

    // Distribute it downwards

    // First create a list of node groups which can potentially manage the request
    list<DecissionInfo> groups;
    // Ignore the zone that has sent this message, only if it is a StructureNode, and zones without information
    if ((msg.isFromEN() || leftChild.addr != src) && leftChild.availInfo.get()) {
        list<DPAvailabilityInformation::AssignmentInfo> ai;
        leftChild.availInfo->getAvailability(ai, req);
        LogMsg("Dsp.Dl", DEBUG) << "Obtained " << ai.size() << " groups with enough availability";
        for (list<DPAvailabilityInformation::AssignmentInfo>::iterator git = ai.begin();
                git != ai.end(); git++) {
            LogMsg("Dsp.Dl", DEBUG) << git->numTasks << " tasks with remaining availability " << git->remngAvail;
            groups.push_back(DecissionInfo(*git, true, branch.getLeftDistance(msg.getRequester())));
        }
    }


    if ((msg.isFromEN() || rightChild.addr != src) && rightChild.availInfo.get()) {
        list<DPAvailabilityInformation::AssignmentInfo> ai;
        rightChild.availInfo->getAvailability(ai, req);
        LogMsg("Dsp.Dl", DEBUG) << "Obtained " << ai.size() << " groups with enough availability";
        for (list<DPAvailabilityInformation::AssignmentInfo>::iterator git = ai.begin();
                git != ai.end(); git++) {
            LogMsg("Dsp.Dl", DEBUG) << git->numTasks << " tasks with remaining availability " << git->remngAvail;
            groups.push_back(DecissionInfo(*git, false, branch.getLeftDistance(msg.getRequester())));
        }
    }

    groups.sort();
    LogMsg("Dsp.Dl", DEBUG) << groups.size() << " groups found";

    // Now divide the request between the zones
    list<DPAvailabilityInformation::AssignmentInfo> leftAssgn, rightAssgn;
    unsigned int leftTasks = 0, rightTasks = 0;
    for (list<DecissionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; it++) {
        LogMsg("Dsp.Dl", DEBUG) << "Using group from " << (it->leftBranch ? "left" : "right") << " branch and " << it->ai.numTasks << " tasks";
        unsigned int tasksInGroup = it->ai.numTasks;
        if (remainingTasks > tasksInGroup) {
            (it->leftBranch ? leftTasks : rightTasks) += tasksInGroup;
            remainingTasks -= tasksInGroup;
            (it->leftBranch ? leftAssgn : rightAssgn).push_back(it->ai);
        } else {
            (it->leftBranch ? leftTasks : rightTasks) += remainingTasks;
            (it->leftBranch ? leftAssgn : rightAssgn).push_back(it->ai);
            (it->leftBranch ? leftAssgn : rightAssgn).back().numTasks = remainingTasks;
            remainingTasks = 0;
        }
    }

    // Update the availability information
    if (leftChild.availInfo.get())
        leftChild.availInfo->update(leftAssgn, req);
    if (rightChild.availInfo.get())
        rightChild.availInfo->update(rightAssgn, req);
    // Send the result to the father
    recomputeInfo();
    notify();

    sendTasks(msg, leftTasks, rightTasks, father.addr == CommAddress());
}
