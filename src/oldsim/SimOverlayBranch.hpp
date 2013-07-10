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

#ifndef SIMOVERLAYBRANCH_H_
#define SIMOVERLAYBRANCH_H_

#include <initializer_list>
#include "OverlayBranch.hpp"
#include "ZoneDescription.hpp"


class SimOverlayBranch : public OverlayBranch {
public:
    virtual bool receiveMessage(const CommAddress & src, const BasicMsg & msg) { return false; }

    virtual bool inNetwork() const { return child[left] != CommAddress(); }

    virtual const CommAddress & getFatherAddress() const { return father; }

    virtual const CommAddress & getChildAddress(int c) const { return child[c]; }

    virtual double getChildDistance(int c, const CommAddress & src) const { return zone[c].distance(src); }

    virtual bool isLeaf(int c) const {
        return zone[c].getMinAddress() == zone[c].getMaxAddress();
    }

    const ZoneDescription & getChildZone(int c) const { return zone[c]; }

    ZoneDescription getZone() const { return ZoneDescription(zone[left], zone[right]); }

    void setFatherAddress(const CommAddress & a) { father = a; }

    void build(const CommAddress & l, bool lb, const CommAddress & r, bool rb);

    friend std::ostream & operator<<(std::ostream& os, const SimOverlayBranch & b) {
        if (b.inNetwork()) {
            os << "f=" << b.father.getIPNum()
                << " l=" << b.child[left].getIPNum()
                << " r=" << b.child[right].getIPNum()
                << " z=" << ZoneDescription(b.zone[left], b.zone[right]);
        } else os << "OFFLINE";
        return os;
    }

    MSGPACK_DEFINE(father, child[left], child[right], zone[left], zone[right]);

private:
    CommAddress father, child[2];
    ZoneDescription zone[2];
};

#endif /* SIMOVERLAYBRANCH_H_ */
