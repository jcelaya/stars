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

#ifndef LINEARAVAILABILITYFUNCTION_H_
#define LINEARAVAILABILITYFUNCTION_H_

#include <list>
#include <utility>
#include <ostream>
#include "Time.hpp"


/**
 * \brief Availability function aproximated with linear interpolation.
 *
 * This class aproximates the available computing
 * before a certain deadline by linear interpolation between the two nearer
 * reference points.
 */
class LinearAvailabilityFunction {
    /**
     * \brief AvailPair of (reference point, available computing).
     *
     * A pair of values which represents the start time of a hole in the function.
     * That is, the starting point of a slope.
     */
    typedef std::pair<Time, unsigned long int> AvailPair;

    std::list<AvailPair> point;   ///< List of pairs (reference point, available computing)
    double power;                 ///< Execution Node computing power

public:
    /**
     * Creates a LinearAvailabilityFunction with a first reference point.
     * @param firstTime First reference point, with default value of 0.
     */
    LinearAvailabilityFunction(double p) : power(p) {}

    void reset(double p) {
        power = p;
        point.clear();
    }

    /**
     * Function like interface to this object, returns the available computing for an hypothetical
     * task which should finish ESTRICTLY BEFORE a certain deadline.
     * @param deadline The deadline of an hypothetical task.
     */
    unsigned long int operator()(Time deadline) const;

    /**
     * Returns the first registered time value. Before that, the operator() always
     * returns 0.
     */
    Time firstTime() const {
        return point.front().first;
    }

    /**
     * Returns whether two LinearAvailabilityFunction objects are equal or not.
     * It is useful to see if the availability has changed.
     */
    bool operator==(const LinearAvailabilityFunction & r) const {
        return power == r.power && point == r.point;
    }

    // Getters and Setters

    /**
     * Returns true if this function's node will be free at certain time.
     * @return True when this is the function of a free node.
     */
    bool isFreeAt(Time when) const {
        return point.empty() || point.rbegin()->first <= when;
    }

    /**
     * Returns the computing power of the execution node.
     * @return The computing power.
     */
    double getPower() const {
        return power;
    }

    /**
     * Adds the starting point of a hole (slope) to the function. It is inserted in increasing
     * order of time.
     * @param p Duration since the last reference point.
     * @param a Availability variation since the last reference point
     */
    void addNewHole(Time p, unsigned long int a);

    friend std::ostream & operator<<(std::ostream & os, const LinearAvailabilityFunction & l);
};

#endif /*LINEARAVAILABILITYFUNCTION_H_*/
