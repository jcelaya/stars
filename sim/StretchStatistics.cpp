/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
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

#include <algorithm>
#include "StretchStatistics.hpp"
#include "TaskBagMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "Task.hpp"
#include "MinStretchScheduler.hpp"
#include "PeerCompNode.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost::posix_time;
using boost::static_pointer_cast;


StretchStatistics::StretchStatistics() : Simulator::InterEventHandler(), lastNonZero(-1) {
	stretch.resize(sim.getNumNodes(), 0.0);
	stretchDesc.resize(sim.getNumNodes());
	for (unsigned int i = 0; i < stretchDesc.size(); i++) {
		stretchDesc[i] = &stretch[i];
	}

	os.open(sim.getResultDir() / fs::path("stretch.stat"));
	os << "# Time, 100%, 80%, 60%, 40%, 20%, 0%, max/min, comment" << endl;
}


static bool compareStretch(double * l, double * r) {
	return *l > *r;
}


void StretchStatistics::afterEvent(const Simulator::Event & ev) {
	// Register the minimum and maximum stretch whenever a new task-bag arrives or finishes
	if ((typeid(*ev.msg) == typeid(TaskBagMsg) &&
			static_pointer_cast<TaskBagMsg>(ev.msg)->isForEN()) ||
		(typeid(*ev.msg) == typeid(TaskStateChgMsg) &&
			static_pointer_cast<TaskStateChgMsg>(ev.msg)->getNewState() == Task::Finished)) {
		// This cast has to be checked before
		double newStretch = static_cast<MinStretchScheduler &>(sim.getNode(ev.to).getScheduler()).getAvailability().getMinimumStretch();
		if (newStretch != 0.0 && stretch[ev.to] == 0.0)
			lastNonZero++;
		else if (newStretch == 0.0 && stretch[ev.to] != 0.0)
			lastNonZero--;
		stretch[ev.to] = newStretch;
		std::sort(stretchDesc.begin(), stretchDesc.end(), compareStretch);
		os << setprecision(3) << (sim.getCurrentTime().getRawDate() / 1000000.0);
		for (int i = 0; i < 5; i++)
			os << ',' << setprecision(8) << fixed << *stretchDesc[i * 0.2 * stretchDesc.size()];
		os << ',' << setprecision(8) << fixed << *stretchDesc.back();
		if (lastNonZero >= 0)
			os << ',' << (*stretchDesc.front() / *stretchDesc[lastNonZero]);
		else os << ",0.00000000";
		if (typeid(*ev.msg) == typeid(TaskBagMsg))
			os << ',' << (1 + static_pointer_cast<TaskBagMsg>(ev.msg)->getLastTask() - static_pointer_cast<TaskBagMsg>(ev.msg)->getFirstTask())
				<< " new tasks accepted at " << ev.to << " for app " << static_pointer_cast<TaskBagMsg>(ev.msg)->getRequestId() << endl;
		else
			os << ",Task ended at " << ev.to << endl;
	}
}
