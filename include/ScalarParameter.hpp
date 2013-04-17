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


template <class T> struct ScalarParameterTraits;

template <class T>
class ScalarParameter {
public:
    typedef typename ScalarParameterTraits<T>::scalar scalar;

    ScalarParameter() : parameter(0), mse(0), linearTerm(0) {}
    ScalarParameter(scalar p) : parameter(p), mse(0), linearTerm(0) {}

    bool operator==(const ScalarParameter<T> & r) const {
        return parameter == r.parameter && mse == r.mse && linearTerm == r.linearTerm;
    }

    double norm(const Interval<scalar> & range, unsigned int count) {
        return mse / (count * range.getExtent() * range.getExtent());
    }

    bool far(const ScalarParameter<T> & r, const Interval<scalar> & range, unsigned int numIntervals) const {
        return range.getExtent() > 0.0 && getInterval(range, numIntervals) != r.getInterval(range, numIntervals);
    }

    unsigned int getInterval(const Interval<scalar> & range, unsigned int numIntervals) const {
        return std::floor((parameter - range.getMin()) * numIntervals / range.getExtent());
    }

    scalar getValue() const {
        return parameter;
    }

    void aggregate(unsigned long count, const ScalarParameter<T> & r, unsigned long rcount) {
        scalar newParameter = T::reduce(parameter, count, r.parameter, rcount);
        double difference = newParameter - parameter, rdifference = newParameter - r.parameter;
        mse += count * difference * difference + 2 * difference * linearTerm
               + r.mse + rcount * rdifference * rdifference + 2 * rdifference * r.linearTerm;
        linearTerm += count * difference + r.linearTerm + rcount * rdifference;
        parameter = newParameter;
    }

    friend std::ostream & operator<<(std::ostream & os, const ScalarParameter<T> & o) {
        return os << o.parameter << 'r' << o.mse << '+' << o.linearTerm;
    }

    MSGPACK_DEFINE(parameter, mse, linearTerm);

private:
    scalar parameter;
    double mse;
    double linearTerm;
};


template <typename scalar>
class MinParameter : public ScalarParameter<MinParameter<scalar> > {
public:
    MinParameter() : ScalarParameter<MinParameter<scalar> >() {}
    MinParameter(scalar value) : ScalarParameter<MinParameter<scalar> >(value) {}

    static scalar reduce(scalar parameter, unsigned long count, scalar rparameter, unsigned long rcount) {
        return parameter < rparameter ? parameter : rparameter;
    }
};


template<typename T> struct ScalarParameterTraits<MinParameter<T> > {
    typedef T scalar;
};


template <typename scalar>
class MaxParameter : public ScalarParameter<MaxParameter<scalar> > {
public:
    MaxParameter() : ScalarParameter<MaxParameter<scalar> >() {}
    MaxParameter(scalar value) : ScalarParameter<MaxParameter<scalar> >(value) {}

    static scalar reduce(scalar parameter, unsigned long count, scalar rparameter, unsigned long rcount) {
        return parameter > rparameter ? parameter : rparameter;
    }
};


template<typename T> struct ScalarParameterTraits<MaxParameter<T> > {
    typedef T scalar;
};


template <typename scalar>
class MeanParameter : public ScalarParameter<MeanParameter<scalar> > {
public:
    MeanParameter() : ScalarParameter<MeanParameter<scalar> >() {}
    MeanParameter(scalar value) : ScalarParameter<MeanParameter<scalar> >(value) {}

    static scalar reduce(scalar parameter, unsigned long count, scalar rparameter, unsigned long rcount) {
        return (parameter * count + rparameter * rcount) / (double)(count + rcount);
    }
};


template<typename T> struct ScalarParameterTraits<MeanParameter<T> > {
    typedef T scalar;
};

#endif /* SCALARPARAMETER_HPP_ */
