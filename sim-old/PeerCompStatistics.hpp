/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2012 Javier Celaya
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

#ifndef PEERCOMPSTATISTICS_H_
#define PEERCOMPSTATISTICS_H_

#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Time.hpp"
class Simulator;


class PeerCompStatistics {
public:
    PeerCompStatistics();

    void saveTotalStatistics() {
        saveCPUStatistics();
    }

    // Public statistics handlers
    void queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd);

private:
    Simulator & sim;

    // Queue Statistics
    void saveQueueLengthStatistics();
    fs::ofstream queueos;
    Time maxQueue;

    // CPU Statistics
    void saveCPUStatistics();
};

#endif /* PEERCOMPSTATISTICS_H_ */
