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

#ifndef INSERTCOMMANDMSG_H_
#define INSERTCOMMANDMSG_H_

#include "BasicMsg.hpp"
#include "CommAddress.hpp"


/**
 * A message to command the ResourceNode to insert itself in the tree.
 */
class InsertCommandMsg : public BasicMsg {
public:
    MESSAGE_SUBCLASS(InsertCommandMsg);

    /// Default constructor
    InsertCommandMsg() {}

    /**
     * Returns the address of the node which is going to receive the InsertMsg.
     * @return The address of the node.
     */
    const CommAddress & getWhere() const {
        return where;
    }

    /**
     * Sets the address of the receiver node.
     * @param p The address of the node.
     */
    void setWhere(const CommAddress & w) {
        where = w;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {
        os << "whr(" << where << ')';
    }

    MSGPACK_DEFINE(where);
private:
    CommAddress where;   ///< The destination node address.
};

#endif /*INSERTCOMMANDMSG_H_*/
