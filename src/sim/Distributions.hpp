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

#ifndef DISTRIBUTIONS_H_
#define DISTRIBUTIONS_H_

#include <deque>
#include <vector>
#include <utility>
#include <ostream>
#include <boost/filesystem/path.hpp>


class Histogram {
    bool calibrating;
    std::vector<double> firstSamples;
    unsigned int limit;

    double first;
    double resolution;
    std::deque<unsigned long int> histogram;
    unsigned long int samples;

public:
    Histogram(double res = 1.0) : calibrating(false), first(0.0), resolution(res), samples(0) {}
    Histogram(unsigned int l) : calibrating(true), limit(l), first(0.0), resolution(0.0), samples(0) {}

    bool isCalibrating() const {
        return calibrating;
    }
    void calibrate();

    void addValue(double value);
    unsigned long int getNumBins() const {
        return histogram.size();
    }
    unsigned long int getSamples() const {
        return samples;
    }
    double getBin(unsigned int i) const {
        return first + i * resolution;
    }
    unsigned long int getSamples(unsigned int i) const {
        return histogram[i];
    }
};


class CDF {
    std::vector<std::pair<double, double> > cdf;

    void optimize();

public:
    CDF() {}
    CDF(Histogram & h) {
        loadFrom(h);
    }
    CDF(const boost::filesystem::path & file) {
        loadFrom(file);
    }

    void loadFrom(Histogram & h);
    void loadFrom(const boost::filesystem::path & file);

    void addValue(double bin, double value);

    double inverse(double x);

    friend std::ostream & operator<<(std::ostream & os, const CDF & c) {
        for (std::vector<std::pair<double, double> >::const_iterator i = c.cdf.begin(); i != c.cdf.end(); i++)
            os << i->first << ',' << i->second << std::endl;
        return os;
    }
};

#endif /*DISTRIBUTIONS_H_*/
