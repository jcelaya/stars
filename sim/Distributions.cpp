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

#include <cmath>
#include <sstream>
#include <algorithm>
#include <boost/filesystem/fstream.hpp>
#include "Distributions.hpp"
using namespace std;
namespace fs = boost::filesystem;

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
        for (vector<double>::iterator it = firstSamples.begin(); it != firstSamples.end(); it++) {
            if (*it < min) min = *it;
            else if (*it > max) max = *it;
        }
        resolution = (max - min) / limit;
        for (vector<double>::iterator it = firstSamples.begin(); it != firstSamples.end(); it++)
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


void CDF::loadFrom(const fs::path & file) {
    fs::ifstream ifs(file);

    string nextLine;
    double bin, freq;
    char delim;

    // Assume good syntax (ends with 1.0, monotonic increasing)
    while (ifs.good()) {
        getline(ifs, nextLine);
        if (!(istringstream(nextLine) >> bin >> delim >> freq).fail())
            cdf.push_back(make_pair(bin, freq));
    }
    optimize();
}


void CDF::addValue(double bin, double value) {
    cdf.push_back(make_pair(bin, value));
    // Assume it is ordered
    int i = cdf.size() - 1;
    for (; i > 0 && bin < cdf[i - 1].first; i--)
        cdf[i] = cdf[i - 1];
    cdf[i] = make_pair(bin, value);
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
    vector<pair<double, double> > result;
    for (vector<pair<double, double> >::iterator it = cdf.begin(); it != cdf.end(); it++) {
        if (it->second != lastprob) {
            result.push_back(*it);
            lastprob = it->second;
        }
    }
    cdf.swap(result);
}
