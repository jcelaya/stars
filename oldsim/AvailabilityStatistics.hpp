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

#ifndef AVAILABILITYSTATISTICS_H_
#define AVAILABILITYSTATISTICS_H_

#include <vector>
#include <string>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "AvailabilityInformation.hpp"
#include "Distributions.hpp"
#include "Time.hpp"

class AvailabilityStatistics {
    /**
     * Description of a change that has arrived to a node. It describes its creation time
     * and the time it arrived at the current node.
     */
    struct Change {
        bool valid;
        Time creation;
        Time end;
        Change() : valid(false) {}
        double duration() const { return (end - creation).seconds(); }
    };

    /// Last changes arrived at every node.
    std::vector<Change> activeChanges;
    Histogram updateTimes;
    Histogram reachedLevel;

    fs::ofstream os;

public:
    AvailabilityStatistics() : updateTimes(0.01), reachedLevel(1.0) {}
    ~AvailabilityStatistics();

    void setNumNodes(unsigned int n) { activeChanges.resize(n); }
    void openStatsFiles(const boost::filesystem::path & statDir);
    void changeUpwards(uint32_t src, uint32_t dst, Time c);
    void finishAvailabilityStatistics();
};

#endif /*AVAILABILITYSTATISTICS_H_*/