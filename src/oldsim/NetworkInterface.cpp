/*
 * NetworkInterface.cpp
 *
 *  Created on: 22/11/2013
 *      Author: javi
 */

#include "NetworkInterface.hpp"

namespace stars {

Duration NetworkInterface::samplingIntervals[2] = {Duration(1.0), Duration(10.0)};


void NetworkInterface::LinkTrafficStats::addMessage(unsigned int size, double bw, Time ref) {
    bytes += size;
    for (int i = 0; i < 2; i++) {
        lastSizes[i].push_back(std::make_pair(size, ref));
        lastBytes[i] += size;
        while (ref - lastSizes[i].front().second >= samplingIntervals[i]) {
            lastBytes[i] -= lastSizes[i].front().first;
            lastSizes[i].pop_front();
        }
        // Adjust last block
        double r = (samplingIntervals[i] - (ref - lastSizes[i].front().second)).seconds() / (size / bw);
        if (r < 1.0) {
            lastBytes[i] -= lastSizes[i].front().first;
            lastSizes[i].front().first *= r;
            lastBytes[i] += lastSizes[i].front().first;
        }
        if (maxBytes[i] < lastBytes[i]) maxBytes[i] = lastBytes[i];
    }
}


void NetworkInterface::LinkTrafficStats::output(std::ostream & os, double bw, double totalTime) const {
    os << bytes << ',' << ((bytes / totalTime) / bw) << ','
            << maxBytes[0] << ',' << (maxBytes[0] / bw) << ','
            << (maxBytes[1] / 10.0) << ',' << ((maxBytes[1] / 10.0) / bw) << ','
            << dataBytes;
}


} /* namespace stars */
