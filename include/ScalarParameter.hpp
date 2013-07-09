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

#ifndef SCALARPARAMETER_HPP_
#define SCALARPARAMETER_HPP_

#include <cmath>
#include <msgpack.hpp>
#include <Interval.hpp>
#include <cstdint>


namespace stars {

template <class T> struct ScalarParameterTraits;

template <class T>
class ScalarParameter {
public:
    typedef typename ScalarParameterTraits<T>::scalar scalar;
    typedef typename ScalarParameterTraits<T>::scalar_max scalar_max;
    typedef Interval<scalar> interval;

    ScalarParameter() : parameter(0), mse(0), linearTerm(0) {}
    ScalarParameter(scalar p) : parameter(p), mse(0), linearTerm(0) {}

    bool operator==(const ScalarParameter<T> & r) const {
        return parameter == r.parameter && mse == r.mse && linearTerm == r.linearTerm;
    }

    double norm(const interval & range, unsigned int count) {
        return range.empty() ? 0.0 : mse / ((double)count * range.getExtent() * range.getExtent());
    }

    bool far(const ScalarParameter<T> & r, const interval & range, unsigned int numIntervals) const {
        return !range.empty() && getInterval(range, numIntervals) != r.getInterval(range, numIntervals);
    }

    unsigned int getInterval(const interval & range, unsigned int numIntervals) const {
        return std::floor((double)interval::difference(parameter, range.getMin()) * numIntervals / range.getExtent());
    }

    scalar getValue() const {
        return parameter;
    }

    void aggregate(unsigned long count, const ScalarParameter<T> & r, unsigned long rcount) {
        scalar newParameter = T::reduce(parameter, count, r.parameter, rcount);
        scalar_max difference = interval::difference(newParameter, parameter),
                rdifference = interval::difference(newParameter, r.parameter);
        mse += count * difference * difference + 2 * difference * linearTerm
               + r.mse + rcount * rdifference * rdifference + 2 * rdifference * r.linearTerm;
        linearTerm += count * difference + r.linearTerm + rcount * rdifference;
        parameter = newParameter;
    }

    friend std::ostream & operator<<(std::ostream & os, const ScalarParameter<T> & o) {
        return os << o.parameter;// << 'r' << o.mse << '+' << o.linearTerm;
    }

    MSGPACK_DEFINE(parameter, mse, linearTerm);

private:
    scalar parameter;
    scalar_max mse;
    scalar_max linearTerm;
};


template <typename scalar, typename scalar_max>
class MinParameter : public ScalarParameter<MinParameter<scalar, scalar_max> > {
public:
    MinParameter() : ScalarParameter<MinParameter<scalar, scalar_max> >() {}
    MinParameter(scalar value) : ScalarParameter<MinParameter<scalar, scalar_max> >(value) {}

    static scalar reduce(scalar parameter, unsigned long count, scalar rparameter, unsigned long rcount) {
        return parameter < rparameter ? parameter : rparameter;
    }
};


template<typename T, typename D> struct ScalarParameterTraits<MinParameter<T, D> > {
    typedef T scalar;
    typedef D scalar_max;
};


template <typename scalar, typename scalar_max>
class MaxParameter : public ScalarParameter<MaxParameter<scalar, scalar_max> > {
public:
    MaxParameter() : ScalarParameter<MaxParameter<scalar, scalar_max> >() {}
    MaxParameter(scalar value) : ScalarParameter<MaxParameter<scalar, scalar_max> >(value) {}

    static scalar reduce(scalar parameter, unsigned long count, scalar rparameter, unsigned long rcount) {
        return parameter > rparameter ? parameter : rparameter;
    }
};


template<typename T, typename D> struct ScalarParameterTraits<MaxParameter<T, D> > {
    typedef T scalar;
    typedef D scalar_max;
};


template <typename scalar, typename scalar_max>
class MeanParameter : public ScalarParameter<MeanParameter<scalar, scalar_max> > {
public:
    MeanParameter() : ScalarParameter<MeanParameter<scalar, scalar_max> >() {}
    MeanParameter(scalar value) : ScalarParameter<MeanParameter<scalar, scalar_max> >(value) {}

    static scalar reduce(scalar parameter, unsigned long count, scalar rparameter, unsigned long rcount) {
        return (parameter * count + rparameter * rcount) / (double)(count + rcount);
    }
};


template<typename T, typename D> struct ScalarParameterTraits<MeanParameter<T, D> > {
    typedef T scalar;
    typedef D scalar_max;
};

}

#endif /* SCALARPARAMETER_HPP_ */
