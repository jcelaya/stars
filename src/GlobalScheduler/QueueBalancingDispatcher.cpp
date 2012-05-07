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

#include "Logger.hpp"
#include "QueueBalancingDispatcher.hpp"
#include "Time.hpp"
#include "ConfigurationManager.hpp"
using boost::shared_ptr;


void QueueBalancingDispatcher::recomputeInfo() {
	LogMsg("Dsp.QB", DEBUG) << "Recomputing the branch information";
	// Only recalculate info for the father
	std::vector<Link>::iterator child = children.begin();
	for (; child != children.end() && !child->availInfo.get(); child++);
	if (child == children.end()) {
		father.waitingInfo.reset();
		return;
	}
	father.waitingInfo.reset(child->availInfo->clone());
	for (child++; child != children.end(); child++)
		if (child->availInfo.get())
			father.waitingInfo->join(*child->availInfo);
	LogMsg("Dsp.QB", DEBUG) << "The result is " << *father.waitingInfo;

	if(!structureNode.isRNChildren()) {
		for (unsigned int i = 0; i < children.size(); i++) {
			LogMsg("Dsp.QB", DEBUG) << "Recomputing the information from the rest of the tree for child " << i;
			// TODO: send full information, based on configuration switch
			std::list<Time> queueLengths;
			if (structureNode.getFather() != CommAddress() && father.availInfo.get())
				queueLengths.push_back(father.availInfo->getMinQueueLength());
			for (unsigned int j = 0; j < children.size(); j++)
				if (j != i && children[j].availInfo.get())
					queueLengths.push_back(children[j].availInfo->getMinQueueLength());
			if (!queueLengths.empty()) {
				Time minQueue = queueLengths.front();
				for (std::list<Time>::iterator it = queueLengths.begin(); it != queueLengths.end(); it++)
					if (minQueue > *it) minQueue = *it;
				if (!children[i].waitingInfo.get() || children[i].waitingInfo->getMinQueueLength() != minQueue) {
					LogMsg("Dsp.QB", DEBUG) << "There were changes with children " << i << ", new min queue is " << minQueue;
					children[i].waitingInfo.reset(new QueueBalancingInfo);
					children[i].waitingInfo->setMinQueueLength(minQueue);
				}
			}
		}
	}
}


/*
 * A block of info associated with a NodeGroup used in the decission algorithm.
 */
struct QueueBalancingDispatcher::DecissionInfo {
	QueueBalancingInfo::MDPTCluster * cluster;
	unsigned int numBranch;
	double distance;
	double availability;
	unsigned int numTasks;

	static const uint32_t ALPHA_MEM = 10;
	static const uint32_t ALPHA_DISK = 1;
	static const uint32_t ALPHA_TIME = 100;

	DecissionInfo(QueueBalancingInfo::MDPTCluster * c, const TaskDescription & req, unsigned int b, double d)
			: cluster(c), numBranch(b), distance(d) {
		availability = ALPHA_MEM * c->getLostMemory(req) + ALPHA_DISK * c->getLostDisk(req) + ALPHA_TIME / (req.getDeadline() - c->maxT).seconds();//c->getLostTime(req);
		numTasks = c->value * ((req.getDeadline() - c->maxT).seconds() * c->minP) / (double)req.getLength();
	}

	bool operator<(const DecissionInfo & r) {
		return availability < r.availability || (availability == r.availability && distance < r.distance);
	}
};


void QueueBalancingDispatcher::handle(const CommAddress & src, const TaskBagMsg & msg) {
	if (msg.isForEN()) return;
	LogMsg("Dsp.QB", INFO) << "Received a TaskBagMsg from " << src;
	if (!structureNode.inNetwork()) {
		LogMsg("Dsp.QB", WARN) << "TaskBagMsg received but not in network";
		return;
	}
	if (!father.waitingInfo.get()) {
		LogMsg("Dsp.QB", WARN) << "TaskBagMsg received but no information!";
		return;
	}

	TaskDescription req = msg.getMinRequirements();
	unsigned int remainingTasks = msg.getLastTask() - msg.getFirstTask() + 1;
	unsigned int nextTask = msg.getFirstTask();
	LogMsg("Dsp.QB", INFO) << "Requested allocation of " << remainingTasks << " tasks with requirements:";
	LogMsg("Dsp.QB", INFO) << "Memory: " << req.getMaxMemory() << "   Disk: " << req.getMaxDisk();
	LogMsg("Dsp.QB", INFO) << "Length: " << req.getLength();
	Time fatherInfo = father.availInfo.get() ? father.availInfo->getMinQueueLength() : Time::getCurrentTime();

	std::list<QueueBalancingInfo::MDPTCluster *> nodeGroups;
	if (structureNode.getFather() != CommAddress()) {
		// Count number of tasks before the minimum length in the rest of the tree
		req.setDeadline(fatherInfo);
		unsigned int tasks = father.waitingInfo->getAvailability(nodeGroups, req);
		LogMsg("Dsp.QB", DEBUG) << "Before the minimum queue (" << fatherInfo << ") there is space for " << tasks << " tasks";

		if (tasks < remainingTasks && (src != structureNode.getFather() || msg.isFromEN())) {
			// Send it up if there are not enough nodes, we are not the root and the sender is not our father or it is a child with the same address
			TaskBagMsg * tbm = msg.clone();
			tbm->setFromEN(false);
			CommLayer::getInstance().sendMessage(structureNode.getFather(), tbm);
			LogMsg("Dsp.QB", INFO) << "Not enough nodes, send to the father";
			return;
		}
	}

	// If there are enough tasks, distribute it downwards
	Time balancedQueue = father.waitingInfo->getAvailability(nodeGroups, remainingTasks, req);
	if (balancedQueue == Time()) {
		LogMsg("Dsp.QB", WARN) << "No node fulfills requirements, dropping!";
		return;
	}
	req.setDeadline(balancedQueue);
	father.waitingInfo->updateAvailability(req);
	LogMsg("Dsp.QB", DEBUG) << "The calculated queue length is " << balancedQueue;

	// Calculate distances
	std::vector<double> distances(children.size(), 1000.0);
	const CommAddress & requester = msg.getRequester();
	for (unsigned int numZone = 0; numZone < children.size(); numZone++) {
		if (children[numZone].addr == requester && !msg.isFromEN()) continue;
		for (StructureNode::zoneConstIterator zone = structureNode.getFirstSubZone();
				zone != structureNode.getLastSubZone(); zone++) {
			if ((*zone)->getLink() == children[numZone].addr && (*zone)->getZone().get()) {
				distances[numZone] = requester.distance((*zone)->getZone()->getMinAddress());
				if (requester.distance((*zone)->getZone()->getMaxAddress()) < distances[numZone])
					distances[numZone] = requester.distance((*zone)->getZone()->getMaxAddress());
				LogMsg("Dsp.QB", DEBUG) << "This zone is at distance " << distances[numZone];
			}
		}
	}

	// First create a list of node groups which can potentially manage the request
	std::list<DecissionInfo> groups;
	for (unsigned int numZone = 0; numZone < children.size(); numZone++) {
		Link & child = children[numZone];
		LogMsg("Dsp.QB", DEBUG) << "Checking zone " << numZone;
		// Ignore zones without information
		if (!child.availInfo.get()) {
			LogMsg("Dsp.QB", DEBUG) << "This zone has no information, skiping";
			continue;
		}
		nodeGroups.clear();
		child.availInfo->getAvailability(nodeGroups, req);
		LogMsg("Dsp.QB", DEBUG) << "Obtained " << nodeGroups.size() << " groups with enough availability";
		for (std::list<QueueBalancingInfo::MDPTCluster *>::iterator git = nodeGroups.begin(); git != nodeGroups.end(); git++) {
			LogMsg("Dsp.QB", DEBUG) << (*git)->value << " tasks of size availability " << req.getLength();
			groups.push_back(DecissionInfo(*git, req, numZone, distances[numZone]));
		}
	}
	LogMsg("Dsp.QB", DEBUG) << groups.size() << " groups found";
	groups.sort();

	// Now divide the request between the zones
	std::vector<unsigned int> numTasks(children.size(), 0);
	for (std::list<DecissionInfo>::iterator it = groups.begin(); it != groups.end() && remainingTasks; it++) {
		LogMsg("Dsp.QB", DEBUG) << "Using group from branch " << it->numBranch << " and " << it->numTasks << " tasks";
		unsigned int tasksInGroup = it->numTasks;
		if (remainingTasks > tasksInGroup) {
			numTasks[it->numBranch] += tasksInGroup;
			remainingTasks -= tasksInGroup;
		} else {
			numTasks[it->numBranch] += remainingTasks;
			remainingTasks = 0;
		}
		it->cluster->maxT = balancedQueue;
	}

	// Now create and send the messages
	for (unsigned int numZone = 0; numZone < children.size(); numZone++) {
		if (numTasks[numZone] > 0) {
			LogMsg("Dsp.QB", INFO) << "Sending " << numTasks[numZone] << " tasks to @" << children[numZone].addr;
			// Create the message
			TaskBagMsg * tbm = msg.clone();
			tbm->setForEN(structureNode.isRNChildren());
			tbm->setFirstTask(nextTask);
			nextTask += numTasks[numZone];
			tbm->setLastTask(nextTask - 1);
			CommLayer::getInstance().sendMessage(children[numZone].addr, tbm);
		}
	}

	// If this branch cannot execute all the tasks, send the request to the father
	if (remainingTasks) {
		LogMsg("Dsp.QB", INFO) << "There are " << remainingTasks << " remaining tasks";
		// Just ignore them
		LogMsg("Dsp.QB", INFO) << "But came from the father.";
	}
}
