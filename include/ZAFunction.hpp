/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2013 Javier Celaya
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

#ifndef ZAFUNCTION_HPP_
#define ZAFUNCTION_HPP_

#include <vector>
#include <utility>
#include <msgpack.hpp>
#include "FSPTaskList.hpp"


namespace stars {

class ZAFunction {
public:
    class SubFunction {
    public:
        SubFunction() : leftEndpoint(minTaskLength), x(0.0), y(0.0), z1(0.0), z2(0.0) {}

        SubFunction(double s, double _x, double _y, double _z1, double _z2) : leftEndpoint(s), x(_x), y(_y), z1(_z1), z2(_z2) {}

        SubFunction(double s, const SubFunction & copy) : leftEndpoint(s), x(copy.x), y(copy.y), z1(copy.z1), z2(copy.z2) {}

        SubFunction(const SubFunction & l, const SubFunction & r, double rightEndpoint);

        void fromThreePoints(double a[3], double b[3]);

        void fromTwoPointsAndSlope(double a[3], double b[3]);

        bool isBiggerThan(const SubFunction & l, const SubFunction & r, double rightEndpoint) const;

        bool covers(double a) const {
            return a >= leftEndpoint;
        }

        double value(double a, int n = 1) const {
            return x / a + y * a * n + z1 * n + z2;
        }

        double slope(double a) const {
            return y - x / (a * a);
        }

        bool operator==(const SubFunction & r) const {
            return leftEndpoint == r.leftEndpoint && x == r.x && y == r.y && z1 == r.z1 && z2 == r.z2;
        }

        bool extends(const SubFunction & l) const {
            return leftEndpoint >= l.leftEndpoint && x == l.x && y == l.y && z1 == l.z1 && z2 == l.z2;
        }

        friend std::ostream & operator<<(std::ostream & os, const SubFunction & o) {
            return os << '(' << o.leftEndpoint << ": L = " << o.x << "/a + " << o.y << "a + " << o.z1 << " + " << o.z2 << ')';
        }

        MSGPACK_DEFINE(leftEndpoint, x, y, z1, z2);

        double leftEndpoint;
        // z1 is the sum of the independent term in L = x/a + z1, while z2 is the independent part in the other functions
        double x, y, z1, z2;   // L = x/a + y*a + z1 + z2
    };

    typedef std::vector<SubFunction> PieceVector;

    enum { minTaskLength = 1000 };

    static void setNumPieces(unsigned int n) {
        numPieces = n;
    }

    /// Default constructor
    ZAFunction() {
        pieces.push_back(SubFunction());
    }

    /**
     * Creates an ZAFunction from a task queue.
     */
    ZAFunction(FSPTaskList curTasks, double power);

    ZAFunction(const ZAFunction & copy) : pieces(copy.pieces) {}

    ZAFunction(ZAFunction && copy) : pieces(std::move(copy.pieces)) {}

    ZAFunction & operator=(const ZAFunction & copy) {
        pieces = copy.pieces;
        return *this;
    }

    ZAFunction & operator=(ZAFunction && copy) {
        pieces = std::move(copy.pieces);
        return *this;
    }

    /**
     * The minimum of two functions.
     * @param l The left function.
     * @param r The right function.
     */
    void min(const ZAFunction & l, const ZAFunction & r);

    /**
     * The maximum of two functions.
     * @param l The left function.
     * @param r The right function.
     */
    void max(const ZAFunction & l, const ZAFunction & r);

    /**
     * The sum of the differences between two functions and the maximum of them
     * @param l The left function.
     * @param r The right function.
     */
    void maxDiff(const ZAFunction & l, const ZAFunction & r, unsigned int lv, unsigned int rv,
                 const ZAFunction & maxL, const ZAFunction & maxR);

    /**
     * Calculates the squared difference with another function
     * @param r The other function.
     * @param sh The measurement horizon for the stretch.
     * @param wh The measurement horizon for the application length.
     */
    double sqdiff(const ZAFunction & r, double ah) const;

    /**
     * Calculates the loss of the approximation to another function, with the least squares method, and the mean
     * of two functions at the same time
     */
    double maxAndLoss(const ZAFunction & l, const ZAFunction & r, unsigned int lv, unsigned int rv,
                      const ZAFunction & maxL, const ZAFunction & maxR, double ah);

    /**
     * Reduces the number of points of the function to a specific number, resulting in a function
     * with approximate value over the original.
     */
    double reduceMax(double horizon, unsigned int quality = 10);

    std::vector<ZAFunction> getReductionOptions(double horizon) const;

    /// Comparison operator
    bool operator==(const ZAFunction & r) const {
        return pieces == r.pieces;
    }

    /// Returns the maximum significant task length
    double getHorizon() const {
        return pieces.empty() ? 0.0 : pieces.back().leftEndpoint;
    }

    const PieceVector & getPieces() const {
        return pieces;
    }

    /**
     * Returns the slowness reached for a certain application type.
     */
    double getSlowness(uint64_t a) const;

    /**
     * Estimates the slowness of allocating n tasks of length a
     */
    double estimateSlowness(uint64_t a, unsigned int n) const;

    /**
     * Reduces the availability when assigning a number of tasks with certain length
     */
    void update(uint64_t length, unsigned int n);

    /// Return the maximum among z1 values
    double getSlowestMachine() const;

    friend std::ostream & operator<<(std::ostream & os, const ZAFunction & o) {
        os << "[LAF";
        for (auto & i: o.pieces) {
            os << ' ' << i;
        }
        return os << ']';
    }

    MSGPACK_DEFINE(pieces);
private:
    // Steps through all the intervals of a pair of functions
    template<int numF, typename Func> static void stepper(const ZAFunction * (&f)[numF], Func step);

    static unsigned int numPieces;

    /// Piece set
    PieceVector pieces;
};

} // namespace stars

#endif /* ZAFUNCTION_HPP_ */
