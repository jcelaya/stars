/*  STaRS, Scalable Task Routing approach to distributed Scheduling
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

#ifndef INTERVAL_HPP_
#define INTERVAL_HPP_

#include <msgpack.hpp>


template <typename limitType>
class Interval {
public:
    Interval() : min(0), max(0) {}

    void setLimits(limitType minAndMax) {
        min = max = minAndMax;
    }

    limitType getMin() const {
        return min;
    }

    limitType getMax() const {
        return max;
    }

    limitType getExtent() const {
        return max - min;
    }

    void extend(const Interval<limitType> & r) {
        if (min > r.min) {
            min = r.min;
        }
        if (max < r.max) {
            max = r.max;
        }
    }

    void extend(limitType r) {
        if (min > r) {
            min = r;
        }
        if (max < r) {
            max = r;
        }
    }

    MSGPACK_DEFINE(min, max);

private:
    limitType min, max;
};


#endif /* INTERVAL_HPP_ */
