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

#include "ConfigurationManager.hpp"
#include "LibStarsStatistics.hpp"
#include "Simulator.hpp"
#include "StarsNode.hpp"
#include "Scheduler.hpp"
#include "Distributions.hpp"


LibStarsStatistics::LibStarsStatistics() : sim(Simulator::getInstance()),
        existingTasks(0), lastTSample(sim.getCurrentTime()), partialFinishedTasks(0), totalFinishedTasks(0) {
    // Queue statistics
    queueos.open(sim.getResultDir() / fs::path("queue_length.stat"));
    queueos << "# Time, max, comment" << std::setprecision(3) << std::fixed << std::endl;
    // Throughput statistics
    throughputos.open(sim.getResultDir() / fs::path("throughput.stat"));
    throughputos << "# Time, tasks finished per second, total tasks finished" << std::endl;
    throughputos << "0,0,0";
}


// Queue statistics
void Scheduler::queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd) {
    Simulator::getInstance().getPCStats().queueChangedStatistics(rid, numAccepted, queueEnd);
}


void LibStarsStatistics::queueChangedStatistics(unsigned int rid, unsigned int numAccepted, Time queueEnd) {
    Time now = sim.getCurrentTime();
    if (maxQueue < queueEnd) {
        queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds()
                << ",queue length updated" << std::endl;
        maxQueue = queueEnd;
        queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds()
                << ',' << numAccepted << " new tasks accepted at " << sim.getCurrentNode().getLocalAddress()
                << " for request " << rid << std::endl;
    }
}


void LibStarsStatistics::finishQueueLengthStatistics() {
    Time now = sim.getCurrentTime();
    queueos << (now.getRawDate() / 1000000.0) << ',' << (maxQueue - now).seconds() << ",end" << std::endl;
}


void LibStarsStatistics::finishThroughputStatistics() {
    Time now = sim.getCurrentTime();
    double elapsed = (now - lastTSample).seconds();
    throughputos << std::setprecision(3) << std::fixed << (now.getRawDate() / 1000000.0) << ',' << (partialFinishedTasks / elapsed) << ',' << totalFinishedTasks << std::endl;
}


void LibStarsStatistics::taskFinished(bool successful) {
    --existingTasks;
    if (successful) {
        partialFinishedTasks++;
        totalFinishedTasks++;
        Time now = sim.getCurrentTime();
        double elapsed = (now - lastTSample).seconds();
        if (elapsed >= delayTSample) {
            throughputos << std::setprecision(3) << std::fixed << (now.getRawDate() / 1000000.0) << ',' << (partialFinishedTasks / elapsed) << ',' << totalFinishedTasks << std::endl;
            partialFinishedTasks = 0;
            lastTSample = now;
        }
    }
}


void LibStarsStatistics::saveCPUStatistics() {
    Simulator & sim = Simulator::getInstance();
    fs::ofstream os(sim.getResultDir() / fs::path("cpu.stat"));
    os << std::setprecision(6) << std::fixed;

    unsigned int maxTasks = 0;
    uint16_t port = ConfigurationManager::getInstance().getPort();
    os << "# Node, tasks exec'd" << std::endl;
    for (unsigned long int addr = 0; addr < sim.getNumNodes(); ++addr) {
        unsigned int executedTasks = sim.getNode(addr).getScheduler().getExecutedTasks();
        os << CommAddress(addr, port) << ',' << executedTasks << std::endl;
        if (executedTasks > maxTasks) maxTasks = executedTasks;
    }
    // End this index
    os << std::endl << std::endl;

    // Next blocks are CDFs
    Histogram executedTasks(maxTasks);
    for (unsigned long int addr = 0; addr < sim.getNumNodes(); ++addr) {
        executedTasks.addValue(sim.getNode(addr).getScheduler().getExecutedTasks());
    }
    os << "# CDF of num of executed tasks" << std::endl << CDF(executedTasks) << std::endl << std::endl;
}

