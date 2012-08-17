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

#ifndef LIBSTARSSTATISTICS_H_
#define LIBSTARSSTATISTICS_H_

#include <list>
#include <utility>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread/mutex.hpp>
#include "Distributions.hpp"
#include "Time.hpp"
class StarsNode;


class LibStarsStatistics {
public:
    LibStarsStatistics();

    /// Open the statistics file at the given directory.
    void openStatsFiles();

    void saveTotalStatistics() {
        saveCPUStatistics();
        finishQueueLengthStatistics();
        finishThroughputStatistics();
        finishAppStatistics();
    }

    // Public statistics handlers
    void queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd);

    void finishedApp(StarsNode & node, long int appId, Time end, int finishedTasks);

    void taskStarted();
    void taskFinished(bool successful);
    unsigned long int getExistingTasks() const { return existingTasks; }

private:
    boost::mutex m;

    // Queue Statistics
    void finishQueueLengthStatistics();
    boost::filesystem::ofstream queueos;
    Time maxQueue;

    // CPU Statistics
    void saveCPUStatistics();

    // Throughput statistics
    void finishThroughputStatistics();
    boost::filesystem::ofstream throughputos;
    unsigned long int existingTasks;
    Time lastTSample;
    unsigned int partialFinishedTasks, totalFinishedTasks;
    static const double delayTSample = 60;

    // App statistics
    void finishAppStatistics();
    boost::filesystem::ofstream appos;
    boost::filesystem::ofstream reqos;
    boost::filesystem::ofstream slowos;
    Histogram numNodesHist;
    Histogram finishedHist;
    Histogram searchHist;
    Histogram jttHist;
    Histogram seqHist;
    Histogram spupHist;
    Histogram slownessHist;
    unsigned int unfinishedApps;
    unsigned int totalApps;
    std::list<std::pair<Time, double> > lastSlowness;
};

#endif /* LIBSTARSSTATISTICS_H_ */
