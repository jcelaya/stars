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

#ifndef DISPATCHCOMMANDMSG_H_
#define DISPATCHCOMMANDMSG_H_

#include "BasicMsg.hpp"
#include "Time.hpp"


/**
 * A command for the SubmissionNode to release a new instance of application.
 */
class DispatchCommandMsg : public BasicMsg {
    /// Set the basic elements for a Serializable class
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(BasicMsg) & appName;
    }

    std::string appName;           ///< Application name
    Time deadline;   ///< Instance deadline

public:
    // This is described in BasicMsg
    virtual DispatchCommandMsg * clone() const {
        return new DispatchCommandMsg(*this);
    }

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

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("DispatchCommandMsg");
    }
};

#endif /*DISPATCHCOMMANDMSG_H_*/
