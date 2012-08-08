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

#include "SimTask.hpp"
#include "Logger.hpp"
#include "TaskStateChgMsg.hpp"
#include "Simulator.hpp"
#include "CommLayer.hpp"
#include "StarsNode.hpp"
using namespace std;
using namespace boost::posix_time;


SimTask::SimTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d) :
		Task(o, reqId, ctid, d), timer(-1) {
	Simulator::getInstance().getPCStats().taskStarted();
	taskDuration = Duration(description.getLength() /
			(double)Simulator::getInstance().getCurrentNode().getAveragePower());
	LogMsg("Sim.Task", DEBUG) << "Created task" << taskId << ", will long " << taskDuration;
}


SimTask::~SimTask() {
	Simulator::getInstance().getPCStats().taskFinished(timer != -1);
}


int SimTask::getStatus() const {
	if (timer == -1) return Prepared;
	else if (finishTime > Simulator::getInstance().getCurrentTime()) return Running;
	else return Finished;
}


void SimTask::run() {
	if (timer == -1) {
		TaskStateChgMsg * tfm = new TaskStateChgMsg;
		tfm->setTaskId(taskId);
		tfm->setOldState(Running);
		tfm->setNewState(Finished);
		timer = Simulator::getInstance().getCurrentNode().setTimer(taskDuration, tfm);
		finishTime = Simulator::getInstance().getCurrentTime() + taskDuration;
		LogMsg("Sim.Task", DEBUG) << "Running task " << taskId << " until " << finishTime;
	}
}


void SimTask::abort() {
	// Cancel timer only if the task is running
	if (timer != -1 && finishTime > Simulator::getInstance().getCurrentTime()) {
		Simulator::getInstance().cancelTimer(timer);
		timer = -1;
	}
}


Duration SimTask::getEstimatedDuration() const {
	if (timer == -1) return taskDuration;
	else if (finishTime > Simulator::getInstance().getCurrentTime())
		return finishTime - Simulator::getInstance().getCurrentTime();
	else return Duration();
}
