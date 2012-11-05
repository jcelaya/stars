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

#include <cmath>
#include <sstream>
#include <algorithm>
#include <boost/filesystem/fstream.hpp>
#include "Distributions.hpp"

void Histogram::addValue(double value) {
    if (calibrating) {
        firstSamples.push_back(value);
        if (firstSamples.size() == limit)
            calibrate();
        else return;
    }

    if (!samples)
        first = resolution > 0.0 ? floor(value / resolution) * resolution : 0.0;
    int i = resolution > 0.0 ? (int)floor((value - first) / resolution) : 0;
    if (i >= (int)histogram.size())
        histogram.resize(i + 1, 0);
    else if (i < 0) {
        histogram.insert(histogram.begin(), -i, 0);
        first += resolution * i;
        i = 0;
    }
    histogram[i]++;
    samples++;
}


void Histogram::calibrate() {
    if (calibrating && !firstSamples.empty()) {
        calibrating = false;
        double min = firstSamples.front(), max = min;
        for (std::vector<double>::iterator it = firstSamples.begin(); it != firstSamples.end(); it++) {
            if (*it < min) min = *it;
            else if (*it > max) max = *it;
        }
        resolution = (max - min) / limit;
        for (std::vector<double>::iterator it = firstSamples.begin(); it != firstSamples.end(); it++)
            addValue(*it);
        firstSamples.clear();
    }
}


void CDF::loadFrom(Histogram & h) {
    h.calibrate();
    cdf.resize(h.getNumBins() + 1);
    double value = 0.0;
    for (unsigned int i = 0; i < h.getNumBins(); i++) {
        value += h.getSamples(i);
        cdf[i].first = h.getBin(i);
        cdf[i].second = value / h.getSamples();
    }
    cdf.back().first = h.getBin(h.getNumBins());
    cdf.back().second = 1.0;
    optimize();
}


void CDF::loadFrom(const boost::filesystem::path & file) {
    boost::filesystem::ifstream ifs(file);

    std::string nextLine;
    double bin, freq;
    char delim;

    // Assume good syntax (ends with 1.0, monotonic increasing)
    while (ifs.good()) {
        std::getline(ifs, nextLine);
        if (!(std::istringstream(nextLine) >> bin >> delim >> freq).fail())
            cdf.push_back(std::make_pair(bin, freq));
    }
    optimize();
}


void CDF::addValue(double bin, double value) {
    cdf.push_back(std::make_pair(bin, value));
    // Assume it is ordered
    int i = cdf.size() - 1;
    for (; i > 0 && bin < cdf[i - 1].first; i--)
        cdf[i] = cdf[i - 1];
    cdf[i] = std::make_pair(bin, value);
}


double CDF::inverse(double x) {
    if (cdf.empty()) return 0.0;
    else if (cdf.size() == 1) return cdf.back().first;
    else if (x <= cdf.front().second) return cdf.front().first;
    // Implemented with a binary search
    unsigned int min = 0, max = cdf.size() - 1;
    while (min + 1 < max) {
        unsigned int med = (max + min) / 2;
        if (cdf[med].second < x) min = med;
        else max = med;
    }
    // Interpolate
    //double a = (cdf[max].first - cdf[min].first) / (cdf[max].second - cdf[min].second);
    //return cdf[min].first + (x - cdf[min].second) * a;
    return cdf[max].first;
}


void CDF::optimize() {
    // Remove sequences of pairs with the same probability
    double lastprob = 0.0;
    std::vector<std::pair<double, double> > result;
    for (std::vector<std::pair<double, double> >::iterator it = cdf.begin(); it != cdf.end(); it++) {
        if (it->second != lastprob) {
            result.push_back(*it);
            lastprob = it->second;
        }
    }
    cdf.swap(result);
}
