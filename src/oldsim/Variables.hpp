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

#ifndef VARIABLES_HPP_
#define VARIABLES_HPP_

#include <cmath>
#include <cstdlib>

// Return a random double in interval (0, 1]
inline double uniform01() {
    return (std::rand() + 1.0) / (RAND_MAX + 1.0);
}

inline double exponential(double mean) {
    return - std::log(uniform01()) * mean;
}

// Return a random double in interval (min, max]
class UniformVariable {
public:
    UniformVariable(double min = 0.0, double max = 1.0) : minimum(min), diff(max - min) {}
    double operator()() {
        return minimum + diff * uniform01();
    }

private:
    double minimum, diff;
};

class ParetoVariable {
public:
    ParetoVariable(double min = 1.0, double _k = 2.0, double max = INFINITY) : xm(min), k(_k), range(std::pow(min / max, _k), 1.0) {}
    double operator()() {
        return xm / std::pow(range(), 1.0 / k);
    }

private:
    double xm, k;
    UniformVariable range;
};

//    static double pareto(double xm, double k, double max) {
//        // xm > 0 and k > 0
//        double r;
//        do
//            r = xm / std::pow(uniform01(), 1.0 / k);
//        while (r > max);
//        return r;
//    }

//    static double normal(double mu, double sigma) {
//        const double pi = 3.14159265358979323846;   //approximate value of pi
//        double r = mu + sigma * std::sqrt(-2.0 * std::log(uniform01())) * std::cos(2.0 * pi * uniform01());
//        return r;
//    }

class DiscreteParetoVariable {
public:
    DiscreteParetoVariable(int min, int max, int s = 1, double k = 2.0) : minimum(min), step(s), p(1.0, k, (max - min) / (double)step + 1.0) {}
    int operator()() {
        return minimum + step * (std::floor(p()) - 1);
    }

private:
    int minimum, step;
    ParetoVariable p;
};

//    static int discretePareto(int min, int max, int step, double k) {
//        int r = min + step * ((int)std::floor(pareto(step, k, max - min) / step) - 1);
//        return r;
//    }

// Return a random int in interval [min, max] with step
class DiscreteUniformVariable {
public:
    DiscreteUniformVariable(int min, int max, int s = 1) : minimum(min - s), diff((int)std::floor((max - min) / (double)s + 1.0)), step(s) {}
    double operator()() {
        return minimum + step * (int)std::ceil(diff * uniform01());
    }

private:
    int minimum, diff, step;
};

#endif /* VARIABLES_HPP_ */
