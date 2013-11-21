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

#ifndef SLAVELOCALSCHEDULER_H_
#define SLAVELOCALSCHEDULER_H_

#include "Scheduler.hpp"
#include "SimOverlayLeaf.hpp"
#include "RescheduleMsg.hpp"

namespace stars {

class SlaveLocalScheduler: public Scheduler {
public:
    SlaveLocalScheduler(OverlayLeaf & l) : Scheduler(l), seq(0) {}

    // This is documented in Scheduler
    virtual AvailabilityInformation * getAvailability() const {
        return nullptr;
    }

private:
    unsigned int seq;
    std::vector<RescheduleMsg::TaskId> taskSequence;

    boost::shared_ptr<Task> getTask(const CommAddress & requester, int64_t rid, uint32_t tid);

    // This is documented in Scheduler
    virtual void reschedule();

    // This is documented in Scheduler
    virtual unsigned int acceptable(const TaskBagMsg & msg);
};

} /* namespace stars */
#endif /* SLAVELOCALSCHEDULER_H_ */
