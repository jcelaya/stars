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

#ifndef LDELTAFUNCTION_HPP_
#define LDELTAFUNCTION_HPP_

#include <list>
#include <vector>
#include <utility>
#include <ostream>
#include "Time.hpp"
#include "Task.hpp"

namespace stars {

class LDeltaFunction {
public:
    typedef std::pair<Time, double> FlopsBeforeDelta;
    typedef std::vector<FlopsBeforeDelta> PieceVector;

    static void setNumPieces(unsigned int n) {
        numPieces = n;
    }

    LDeltaFunction() : slope(0.0) {}

    LDeltaFunction(double power, const std::list<std::shared_ptr<Task> > & queue);

    LDeltaFunction(const LDeltaFunction & copy) : points(copy.points), slope(copy.slope) {}

    LDeltaFunction(LDeltaFunction && move) : slope(move.slope) {
        points.swap(move.points);
    }

    LDeltaFunction & operator=(const LDeltaFunction & copy) {
        slope = copy.slope;
        points = copy.points;
        return *this;
    }

    LDeltaFunction & operator=(LDeltaFunction && move) {
        slope = move.slope;
        points = std::move(move.points);
        return *this;
    }

    /**
     * Returns the last slope of this function
     */
    double getSlope() const {
        return slope;
    }

    /**
     * Creates a function from the aggregation of two other. The result is a conservative approximation of the
     * sum of functions.
     * @param l The left function.
     * @param r The right function.
     */
    void min(const LDeltaFunction & l, const LDeltaFunction & r);

    /**
     * Creates a function from the aggregation of two other. The result is an optimistic approximation of the
     * sum of functions.
     * @param l The left function.
     * @param r The right function.
     */
    void max(const LDeltaFunction & l, const LDeltaFunction & r);

    /**
     * Calculates the squared difference with another function
     * @param r The other function
     * @param ref the initial time
     * @param h The horizon of measurement
     */
    double sqdiff(const LDeltaFunction & r, const Time & ref, const Time & h) const;

    /**
     * Calculates the loss of the approximation to another function, with the least squares method, and the minimum
     * of two functions at the same time
     */
    double minAndLoss(const LDeltaFunction & l, const LDeltaFunction & r, unsigned int lv, unsigned int rv, const LDeltaFunction & lc, const LDeltaFunction & rc,
                      const Time & ref, const Time & h);

    /**
     * Calculates the linear combination of two functions
     * @param f Array of functions
     * @param c Array of function coefficients
     */
    void lc(const LDeltaFunction & l, const LDeltaFunction & r, double lc, double rc);

    /**
     * Reduces the number of points of the function to a specific number, resulting in a function
     * with lower or equal value to the original.
     * @param v Number of nodes it represents
     * @param c Maximum function among the clustered ones.
     * @param ref the initial time
     * @param h The horizon of measurement
     */
    double reduceMin(unsigned int v, LDeltaFunction & c, const Time & ref, const Time & h, unsigned int quality = 10);

    /**
     * Reduces the number of points of the function to a specific number, resulting in a function
     * with greater or equal value to the original.
     * Unlike the previous method, this one assumes that v = 1 and c is the null function.
     * @param ref the initial time
     * @param h The horizon of measurement
     */
    double reduceMax(const Time & ref, const Time & h, unsigned int quality = 10);

    bool operator==(const LDeltaFunction & r) const {
        return slope == r.slope && points == r.points;
    }

    bool isFree() const {
        return points.empty();
    }

    Time getHorizon() const {
        return points.empty() ? Time::getCurrentTime() + Duration(1.0) : points.back().first;
    }

    double getAvailabilityBefore(Time delta) const;

    /**
     * Reduces the availability when assigning a task with certain length and deadline
     */
    void update(uint64_t length, Time deadline, Time horizon);

    const PieceVector & getPoints() const {
        return points;
    }

    friend std::ostream & operator<<(std::ostream & os, const LDeltaFunction & o) {
        for (auto & i : o.points)
            os << '(' << i.first << ',' << i.second << "),";
        return os << o.slope;
    }

    MSGPACK_DEFINE(points, slope);

private:
    // Steps through a vector of functions, with all their slope-change points, and the points where the two first functions cross
    template<int numF, class S> static void stepper(const LDeltaFunction * (&f)[numF],
                                                    const Time & ref, const Time & h, S & step);

    static unsigned int numPieces;

    /// Function points defining segments
    PieceVector points;
    double slope;   ///< Slope at the end of the function
};

}

#endif /* LDELTAFUNCTION_HPP_ */
