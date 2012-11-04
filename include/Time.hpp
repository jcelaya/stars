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

#ifndef TIME_H_
#define TIME_H_

#include <ostream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <msgpack.hpp>

// Time classes. boost::posix_time is good, but slow for simulation purposes

/**
 * A time lapse
 */
class Duration {
public:
    explicit Duration(int64_t micro = 0) : d(micro) {}
    explicit Duration(double seconds) : d(seconds * 1000000.0) {}
    bool operator==(const Duration & r) const {
        return d == r.d;
    }
    bool operator!=(const Duration & r) const {
        return d != r.d;
    }
    bool operator<(const Duration & r) const {
        return d < r.d;
    }
    bool operator>(const Duration & r) const {
        return d > r.d;
    }
    bool operator<=(const Duration & r) const {
        return d <= r.d;
    }
    bool operator>=(const Duration & r) const {
        return d >= r.d;
    }
    Duration operator+(const Duration & r) const  {
        return Duration(d + r.d);
    }
    Duration & operator+=(const Duration & r) {
        d += r.d;
        return *this;
    }
    Duration operator-(const Duration & r) const {
        return Duration(d - r.d);
    }
    Duration & operator-=(const Duration & r) {
        d -= r.d;
        return *this;
    }
    Duration operator*(double m) const {
        return Duration((int64_t)(d * m));
    }
    Duration & operator*=(double m) {
        d *= m;
        return *this;
    }
    double seconds() const {
        return d / 1000000.0;
    }
    int64_t microseconds() const {
        return d;
    }
    bool is_negative() const {
        return d < 0;
    }
    friend std::ostream & operator<<(std::ostream & os, const Duration & r);

    MSGPACK_DEFINE(d);
private:
    int64_t d;
};


/**
 * A time value
 */
class Time {
public:
    explicit Time(int64_t rawDate = 0) : t(rawDate) {}
    explicit Time(boost::posix_time::ptime time) {
        from_posix_time(time);
    }
    bool operator==(const Time & r) const {
        return t == r.t;
    }
    bool operator!=(const Time & r) const {
        return t != r.t;
    }
    bool operator<(const Time & r) const {
        return t < r.t;
    }
    bool operator>(const Time & r) const {
        return t > r.t;
    }
    bool operator<=(const Time & r) const {
        return t <= r.t;
    }
    bool operator>=(const Time & r) const {
        return t >= r.t;
    }
    Duration operator-(const Time & r) const {
        return Duration(t - r.t);
    }
    Time operator+(const Duration & r) const {
        return Time(t + r.microseconds());
    }
    Time & operator+=(const Duration & r) {
        t += r.microseconds();
        return *this;
    }
    Time operator-(const Duration & r) const {
        return Time(t - r.microseconds());
    }
    Time & operator-=(const Duration & r) {
        t -= r.microseconds();
        return *this;
    }
    friend std::ostream & operator<<(std::ostream & os, const Time & r);
    boost::posix_time::ptime to_posix_time() const;
    void from_posix_time(boost::posix_time::ptime time);
    int64_t getRawDate() const {
        return t;
    }
    /**
     * Returns the current time in UTC as a Time value.
     * @return The current time.
     */
    static Time getCurrentTime();

    MSGPACK_DEFINE(t);
private:
    int64_t t;
};

#endif /* TIME_H_ */
