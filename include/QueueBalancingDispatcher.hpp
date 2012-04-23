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

#ifndef QUEUEBALANCINGDISPATCHER_H_
#define QUEUEBALANCINGDISPATCHER_H_

#include "Dispatcher.hpp"
#include "QueueBalancingInfo.hpp"
#include "Time.hpp"


/**
 * \brief Resource request dispatcher for execution node requests.
 *
 * This Dispatcher receives TaskBagMsg requests to assign tasks to execution nodes. It also
 * controls the aggregation of TimeConstraintInfo objects in this branch of the tree.
 */
class QueueBalancingDispatcher : public Dispatcher<QueueBalancingInfo> {
public:
	/**
	 * Constructs a QueueBalancingDispatcher and associates it with the corresponding StructureNode.
	 * @param sn The StructureNode of this branch.
	 */
	QueueBalancingDispatcher(StructureNode & sn) : Dispatcher(sn) {}

private:
	struct DecissionInfo;

	// This is documented in Dispatcher.
	virtual void handle(const CommAddress & src, const TaskBagMsg & msg);

	// This is documented in Dispatcher.
	virtual void recomputeInfo();
};

#endif /*QUEUEBALANCINGDISPATCHER_H_*/
