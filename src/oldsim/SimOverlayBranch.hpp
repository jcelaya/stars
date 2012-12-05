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

#include "OverlayBranch.hpp"
#include "ZoneDescription.hpp"


class SimOverlayBranch : public OverlayBranch {
public:
    virtual bool receiveMessage(const CommAddress & src, const BasicMsg & msg) { return false; }

    virtual bool inNetwork() const { return left != CommAddress(); }

    virtual const CommAddress & getFatherAddress() const { return father; }

    virtual const CommAddress & getLeftAddress() const { return left; }

    virtual double getLeftDistance(const CommAddress & src) const { return leftZone.distance(src); }

    virtual bool isLeftLeaf() const {
        return leftZone.getMinAddress() == leftZone.getMaxAddress();
    }

    virtual const CommAddress & getRightAddress() const { return right; }

    virtual double getRightDistance(const CommAddress & src) const { return rightZone.distance(src); }

    virtual bool isRightLeaf() const {
        return rightZone.getMinAddress() == rightZone.getMaxAddress();
    }

    const ZoneDescription & getLeftZone() const { return leftZone; }

    const ZoneDescription & getRightZone() const { return rightZone; }

    ZoneDescription getZone() const { return ZoneDescription(leftZone, rightZone); }

    void setFatherAddress(const CommAddress & a) { father = a; }

    void build(const CommAddress & l, bool lb, const CommAddress & r, bool rb);

    friend std::ostream & operator<<(std::ostream& os, const SimOverlayBranch & b) {
        if (b.inNetwork()) {
            os << "f=" << b.father << " l=" << b.left << " r=" << b.right << " z=" << ZoneDescription(b.leftZone, b.rightZone);
        } else os << "OFFLINE";
        return os;
    }

    MSGPACK_DEFINE(father, left, right, leftZone, rightZone);

private:
    CommAddress father, left, right;
    ZoneDescription leftZone, rightZone;
};

#endif /* SIMOVERLAYBRANCH_H_ */
