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

#ifndef DISPATCHCOMMANDMSG_H_
#define DISPATCHCOMMANDMSG_H_

#include "BasicMsg.hpp"
#include "Time.hpp"


/**
 * A command for the SubmissionNode to release a new instance of application.
 */
class DispatchCommandMsg : public BasicMsg {
public:
    MESSAGE_SUBCLASS(DispatchCommandMsg);

    /**
     * Returns the minimum resource requirements for all the tasks requested.
     * @return A task description with the most restrictive requirements.
     */
    const std::string & getAppName() const {
        return appName;
    }

    /**
     * Sets the number of tasks to be allocated.
     * @param nt Number of tasks.
     */
    void setAppName(const std::string & n) {
        appName = n;
    }

    /**
     * Returns the deadline of the task.
     */
    Time getDeadline() const {
        return deadline;
    }

    /**
     * Sets the deadline of the task.
     */
    void setDeadline(Time d) {
        deadline = d;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {
        os << "app(" << appName << ") dl(" << deadline << ')';
    }

    MSGPACK_DEFINE(appName, deadline);
private:
    std::string appName;           ///< Application name
    Time deadline;   ///< Instance deadline
};

#endif /*DISPATCHCOMMANDMSG_H_*/
