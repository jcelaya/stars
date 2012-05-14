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

#ifndef BASICMSG_H_
#define BASICMSG_H_

#include <ostream>
#include <iomanip>
#include "Serializable.hpp"

/**
 * \brief Basic message class, with a service identifier.
 *
 * This is the base class for any message sent through the network. It is received by the CommLayer object and sent
 * to the handler that registered with the message class.
 */
class BasicMsg {
    /// Set the basic elements for a Serializable class
    SRLZ_API SRLZ_METHOD() {}

public:
    virtual ~BasicMsg() {}

    /**
     * Produces an exact copy of this object, regardless of its actual class.
     * @returns A pointer to a base class.
     */
    virtual BasicMsg * clone() const = 0;

    /**
     * Provides a textual representation of this object. Just one line, no endline char.
     * @param os Output stream where to write the representation.
     */
    virtual void output(std::ostream& os) const {}

    /**
     * Provides the name of this message
     */
    virtual std::string getName() const = 0;

private:
    // Forbid assignment
    BasicMsg & operator=(const BasicMsg &);
};

inline std::ostream & operator<<(std::ostream& os, const BasicMsg & s) {
    std::ios_base::fmtflags f = os.flags();
    os << std::setw(0) << s.getName() << ": ";
    s.output(os);
    return os << std::setiosflags(f);
}


#endif /*BASICMSG_H_*/
