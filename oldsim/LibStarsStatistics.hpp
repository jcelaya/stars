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

#ifndef LIBSTARSSTATISTICS_H_
#define LIBSTARSSTATISTICS_H_

#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Time.hpp"
class Simulator;


class LibStarsStatistics {
public:
    LibStarsStatistics();

    void saveTotalStatistics() {
        saveCPUStatistics();
        finishQueueLengthStatistics();
        finishThroughputStatistics();
    }

    // Public statistics handlers
    void queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd);

    void taskStarted() { ++existingTasks; }
    void taskFinished(bool successful);
    unsigned long int getExistingTasks() const { return existingTasks; }

private:
    Simulator & sim;

    // Queue Statistics
    void finishQueueLengthStatistics();
    fs::ofstream queueos;
    Time maxQueue;

    // CPU Statistics
    void saveCPUStatistics();

    // Throughput statistics
    void finishThroughputStatistics();
    fs::ofstream throughputos;
    unsigned long int existingTasks;
    Time lastTSample;
    unsigned int partialFinishedTasks, totalFinishedTasks;
    static const double delayTSample = 60;

};

#endif /* LIBSTARSSTATISTICS_H_ */
