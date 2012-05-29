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

#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include <map>
#include <vector>
#include <cmath>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <msg/msg.h>
#include "BasicMsg.hpp"
#include "PeerCompNode.hpp"
#include "Properties.hpp"
#include "Time.hpp"
#include "PeerCompStatistics.hpp"
#include "PerformanceStatistics.hpp"
class PerfectScheduler;
class SimulationCase;


class Simulator {
public:
    static Simulator & getInstance() {
        static Simulator instance;
        return instance;
    }

    bool run(Properties & property);
    
    /**
     * The entry point for every process.
     */
    static int processFunction(int argc, char * argv[]);

    unsigned long int getNumNodes() const {
        return routingTable.size();
    }
    
    PeerCompNode & getNode(unsigned int i) {
        return routingTable[i];
    }
    
    const boost::filesystem::path & getResultDir() const {
        return resultDir;
    }
    
    static PeerCompNode & getCurrentNode() {
        return *static_cast<PeerCompNode *>(MSG_host_get_data(MSG_host_self()));
    }
    
//     PerformanceStatistics & getPStats() {
//         return pstats;
//     }
//     PeerCompStatistics & getPCStats() {
//         return pcstats;
//     }

    // Network methods
    unsigned int sendMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg);
    
    unsigned int injectMessage(uint32_t src, uint32_t dst, boost::shared_ptr<BasicMsg> msg,
                               Duration d = Duration(0.0), bool withOpDuration = false);

    // Time methods
    static Time getCurrentTime() {
        return Time(int64_t(MSG_get_clock() * 1000000));
    }
    
    boost::posix_time::time_duration getRealTime() const {
        return boost::posix_time::microsec_clock::local_time() - simStart;
    }

    int setTimer(uint32_t dst, Time when, boost::shared_ptr<BasicMsg> msg);

    void cancelTimer(int timerId);

    // Log methods
    void progressLog(const char * msg);

    void log(const std::string & category, int priority, LogMsg::AbstractTypeContainer * values);
    
    // Return a random double in interval (0, 1]
    static double uniform01() {
        double r = (std::rand() + 1.0) / (RAND_MAX + 1.0);
        return r;
    }

    // Return a random double in interval (min, max]
    static double uniform(double min, double max) {
        double r = min + (max - min) * uniform01();
        return r;
    }

    static double exponential(double mean) {
        double r = - std::log(uniform01()) * mean;
        return r;
    }

    static double pareto(double xm, double k, double max) {
        // xm > 0 and k > 0
        double r;
        do
            r = xm / std::pow(uniform01(), 1.0 / k);
        while (r > max);
        return r;
    }

    static double normal(double mu, double sigma) {
        const double pi = 3.14159265358979323846;   //approximate value of pi
        double r = mu + sigma * std::sqrt(-2.0 * std::log(uniform01())) * std::cos(2.0 * pi * uniform01());
        return r;
    }

    static int discretePareto(int min, int max, int step, double k) {
        int r = min + step * ((int)std::floor(pareto(step, k, max - min) / step) - 1);
        return r;
    }

    // Return a random int in interval [min, max] with step
    static int uniform(int min, int max, int step = 1) {
        int r = min + step * ((int)std::ceil(std::floor((double)(max - min) / (double)step + 1.0) * uniform01()) - 1);
        return r;
    }

private:
    // Helpers
    unsigned long int getMsgSize(boost::shared_ptr<BasicMsg> msg);
    
    // Simulation framework
    std::vector<PeerCompNode> routingTable;
    std::string platformFile;
    PeerCompNodeFactory nodeFactory;
    boost::shared_ptr<SimulationCase> simCase;
    
    boost::filesystem::path resultDir;
    boost::filesystem::ofstream progressFile, debugFile;
    boost::iostreams::filtering_ostream debugArchive;

    // Simulation global statistics
    // TODO: beware concurrency
    //PerformanceStatistics pstats;
    //PeerCompStatistics pcstats;
    boost::posix_time::ptime simStart, opStart;
    boost::posix_time::time_duration maxRealTime;
    Duration maxSimTime;
    unsigned int maxMemUsage;
    unsigned int showStep;

    // Singleton
    Simulator() {}
};

#endif /*SIMULATOR_H_*/
