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

#ifndef FSPTASKBAGMSG_H_
#define FSPTASKBAGMSG_H_

#include "TaskBagMsg.hpp"

namespace stars {

class FSPTaskBagMsg: public TaskBagMsg {
public:
    FSPTaskBagMsg() : slowness(0.0) {}
    FSPTaskBagMsg(const TaskBagMsg & copy) : TaskBagMsg(copy), slowness(0.0) {}
    FSPTaskBagMsg(const FSPTaskBagMsg & copy) : TaskBagMsg(copy), slowness(copy.slowness) {}

    MESSAGE_SUBCLASS(FSPTaskBagMsg);

    double getEstimatedSlowness() const { return slowness; }
    void setEstimatedSlowness(double s) { slowness = s; }

    MSGPACK_DEFINE((TaskBagMsg &)*this, slowness);

private:
    double slowness;
};

} /* namespace stars */

#endif /* FSPTASKBAGMSG_H_ */
