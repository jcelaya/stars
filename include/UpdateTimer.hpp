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

#ifndef UPDATETIMER_H_
#define UPDATETIMER_H_

#include "BasicMsg.hpp"


/**
 * A message to signal that an update of the availability information can be sent
 * to the father without using more bandwidth than expected.
 */
class UpdateTimer : public BasicMsg {
public:
    MESSAGE_SUBCLASS(UpdateTimer);

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE();
};

#endif /*UPDATETIMER_H_*/
