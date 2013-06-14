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

#ifndef LAFUNCTION_HPP_
#define LAFUNCTION_HPP_

#include <vector>
#include <utility>
#include <msgpack.hpp>
#include "TaskProxy.hpp"


namespace stars {

class LAFunction {
public:
    class SubFunction {
    public:
        SubFunction() : x(0.0), y(0.0), z1(0.0), z2(0.0) {}

        SubFunction(double _x, double _y, double _z1, double _z2) : x(_x), y(_y), z1(_z1), z2(_z2) {}

        double value(double a, int n = 1) const {
            return x / a + y * a * n + z1 * n + z2;
        }

        bool operator==(const SubFunction & r) const {
            return x == r.x && y == r.y && z1 == r.z1 && z2 == r.z2;
        }

        bool operator!=(const SubFunction & r) const {
            return !operator==(r);
        }

        friend std::ostream & operator<<(std::ostream & os, const SubFunction & o) {
            return os << "L = " << o.x << "/a + " << o.y << "a + " << o.z1 << " + " << o.z2;
        }

        MSGPACK_DEFINE(x, y, z1, z2);

        // z1 is the sum of the independent term in L = x/a + z1, while z2 is the independent part in the other functions
        double x, y, z1, z2;   // L = x/a + y*a + z1 + z2
    };

    typedef std::vector<std::pair<double, SubFunction> > PieceVector;

    enum { minTaskLength = 1000 };

    /// Sets the number of reference points in each function.
    static void setNumPieces(unsigned int n) {
        numPieces = n;
    }

    /// Default constructor
    LAFunction() {
        pieces.push_back(std::make_pair(minTaskLength, SubFunction()));
    }

    /**
     * Creates an LAFunction from a task queue.
     */
    LAFunction(TaskProxy::List curTasks,
            const std::vector<double> & switchValues, double power);

    LAFunction(const LAFunction & copy) : pieces(copy.pieces) {}

    LAFunction(LAFunction && copy) : pieces(std::move(copy.pieces)) {}

    LAFunction & operator=(const LAFunction & copy) {
        pieces = copy.pieces;
        return *this;
    }

    LAFunction & operator=(LAFunction && copy) {
        pieces = std::move(copy.pieces);
        return *this;
    }

    /**
     * The minimum of two functions.
     * @param l The left function.
     * @param r The right function.
     */
    void min(const LAFunction & l, const LAFunction & r);

    /**
     * The maximum of two functions.
     * @param l The left function.
     * @param r The right function.
     */
    void max(const LAFunction & l, const LAFunction & r);

    /**
     * The sum of the differences between two functions and the maximum of them
     * @param l The left function.
     * @param r The right function.
     */
    void maxDiff(const LAFunction & l, const LAFunction & r, unsigned int lv, unsigned int rv,
                 const LAFunction & maxL, const LAFunction & maxR);

    /**
     * Calculates the squared difference with another function
     * @param r The other function.
     * @param sh The measurement horizon for the stretch.
     * @param wh The measurement horizon for the application length.
     */
    double sqdiff(const LAFunction & r, double ah) const;

    /**
     * Calculates the loss of the approximation to another function, with the least squares method, and the mean
     * of two functions at the same time
     */
    double maxAndLoss(const LAFunction & l, const LAFunction & r, unsigned int lv, unsigned int rv,
                      const LAFunction & maxL, const LAFunction & maxR, double ah);

    /**
     * Reduces the number of points of the function to a specific number, resulting in a function
     * with approximate value over the original.
     */
    double reduceMax(unsigned int v, double ah, unsigned int quality = 10);

    /// Comparison operator
    bool operator==(const LAFunction & r) const {
        return pieces == r.pieces;
    }

    /// Returns the maximum significant task length
    double getHorizon() const {
        return pieces.empty() ? 0.0 : pieces.back().first;
    }

    const std::vector<std::pair<double, SubFunction> > & getPieces() const {
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

    friend std::ostream & operator<<(std::ostream & os, const LAFunction & o) {
        os << "[LAF";
        for (std::vector<std::pair<double, SubFunction> >::const_iterator i = o.pieces.begin(); i != o.pieces.end(); i++) {
            os << " (" << i->first << ", " << i->second << ')';
        }
        return os << ']';
    }

    MSGPACK_DEFINE(pieces);
private:
    // Steps through all the intervals of a pair of functions
    template<int numF, typename Func> static void stepper(const LAFunction * (&f)[numF], Func step);

    static unsigned int numPieces;

    /// Piece set
    PieceVector pieces;
};

} // namespace stars

#endif /* LAFUNCTION_HPP_ */
