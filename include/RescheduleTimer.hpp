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

#ifndef RESCHEDULETIMER_H_
#define RESCHEDULETIMER_H_

#include "BasicMsg.hpp"


/**
 * A message to signal that a reschedule is needed in order to check deadlines
 * and provide the father with fresh information
 */
class RescheduleTimer: public BasicMsg {
public:
    MESSAGE_SUBCLASS(RescheduleTimer);

    /// Default constructor
    //RescheduleTimer() {}

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    EMPTY_MSGPACK_DEFINE();
};

#endif /* RESCHEDULETIMER_H_ */
