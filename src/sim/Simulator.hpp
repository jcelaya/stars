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

#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include <vector>
#include <cmath>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/thread/mutex.hpp>
#include <msg/msg.h>
#include "StarsNode.hpp"
#include "SimulationCase.hpp"
#include "Time.hpp"
#include "LibStarsStatistics.hpp"
#include "PerformanceStatistics.hpp"
class BasicMsg;


/**
 * Main simulator class.
 *
 * This class interfaces the rest of the simulation components and the STaRS library
 * with SimGrid.
 */
class Simulator {
public:
    /**
     * Returns the singleton instance.
     */
    static Simulator & getInstance() {
        static Simulator instance;
        return instance;
    }

    /**
     * Run a simulation case.
     *
     * @param confFile The path to the configuration file for the case.
     */
    bool run(const std::string & confFile);

    void stop() { end = true; }

    bool doContinue();

    /**
     * Returns the SimulationCase instance that is being run.
     */
    SimulationCase & getCase() { return *simCase; }

    /**
     * Returns the number of nodes in the table. This is actually the number of processes,
     * which in practice is the same as the number of hosts.
     */
    unsigned long int getNumNodes() const {
        return xbt_dynar_length(MSG_hosts_as_dynar());
    }

    StarsNode & getNode(unsigned int i) {
        return routingTable[i];
    }

    /**
     * Returns the path of the directory containing the result files, such as logs.
     */
    const boost::filesystem::path & getResultDir() const {
        return resultDir;
    }

    bool isInSimulation() const {
        return inSimulation;
    }

    /**
     * Returns the StarsNode instance associated to the current process.
     */
    StarsNode & getCurrentNode() {
        if (inSimulation)
            return *static_cast<StarsNode *>(MSG_host_get_data(MSG_host_self()));
        else return routingTable[currentNode];
    }

    /**
     * Returns the PerformanceStatistics object, to record a new event.
     */
    PerformanceStatistics & getPerformanceStatistics() {
        return pstats;
    }

    LibStarsStatistics & getStarsStatistics() {
        return starsStats;
    }

    /**
     * Returns the elapsed time since the simulation started. It does not take into account
     * the time needed to prepare the simulation case.
     */
    boost::posix_time::time_duration getRealTime() const {
        return boost::posix_time::microsec_clock::local_time() - simStart;
    }

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
    // Log methods
    friend class LogMsg;

    /**
     * Logs a message to the log file.
     * @param category The category of the message, with subcategories separated with dots.
     * @param priority The priority of the message, as stated by log4cpp.
     * @param values The list of values to be logged.
     */
    void log(const char *  category, int priority, LogMsg::AbstractTypeContainer * values);

    // Simulation framework
    boost::scoped_array<StarsNode> routingTable;   ///< The set of StarsNodes in this simulation.
    boost::scoped_ptr<SimulationCase> simCase;     ///< The current simulation case. Currently, only one is allowed per execution.
    bool end;                                      ///< Signals the end of the simulation
    bool inSimulation;                             ///< States if the currently running code is inside MSG_main
    uint32_t currentNode;                          ///< Current node index when not inside MSG_main

    boost::filesystem::path resultDir;                  ///< The path to the directory that contains the results.
    boost::filesystem::ofstream debugFile;              ///< The log4cpp file.
    boost::iostreams::filtering_ostream debugArchive;   ///< GZip filter for the log file.
    boost::mutex debugMutex;                            ///< Mutex to write to the log file.

    // Simulation global statistics
    PerformanceStatistics pstats;
    LibStarsStatistics starsStats;
    boost::posix_time::ptime simStart;              ///< Start time of the simulation.
    boost::posix_time::ptime nextProgress;          ///< Time to show next progress step
    boost::posix_time::ptime opStart;
    boost::posix_time::time_duration maxRealTime;   ///< Foo
    Duration maxSimTime;
    unsigned int maxMemUsage;
    unsigned int showStep;
    boost::mutex stepMutex;

    /// Default constructor, it's private to disallow instantiation outside getInstance() method.
    Simulator() {}
};

#endif /*SIMULATOR_H_*/
