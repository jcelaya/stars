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

#ifndef FSPAVAILABILITYINFORMATION_H_
#define FSPAVAILABILITYINFORMATION_H_

#include <cmath>
#include <vector>
#include <list>
#include <utility>
#include <ostream>

#include "AvailabilityInformation.hpp"
#include "ClusteringList.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
#include "FSPTaskList.hpp"
#include "ZAFunction.hpp"
#include "ScalarParameter.hpp"

namespace stars {

/**
 * \brief Information about how slowness changes when a new application arrives.
 *
 * This class provides information about how the stretch in a certain set of nodes changes
 * when tasks of a new application are assigned to them.
 */
class FSPAvailabilityInformation : public AvailabilityInformation {
public:
    /**
     * \brief A cluster of availability function with fair allocation constraints
     *
     * This cluster contains an aggregation of availability functions with memory, disk and fair allocation constraints.
     */
    class MDZCluster {
    public:
        /// Default constructor, do nothing for fast empty array creation
        MDZCluster() {}
        /// Creates a cluster for a certain information object r and a set of initial values
        MDZCluster(uint32_t m, uint32_t d, const FSPTaskList & curTasks, double power)
                : reference(NULL), value(1), minM(m), minD(d), maxZ(curTasks, power), accumZsq(0.0), accumZmax(maxZ) {}
        MDZCluster(const MDZCluster & copy) {
            *this = copy;
        }

        MDZCluster & operator=(const MDZCluster & r) {
            reference = r.reference;
            value = r.value;
            minM = r.minM;
            minD = r.minD;
            maxZ = r.maxZ;
            accumZsq = r.accumZsq;
            accumZmax = r.accumZmax;
            return *this;
        }

        MDZCluster & operator=(MDZCluster && r) {
            reference = r.reference;
            value = r.value;
            minM = r.minM;
            minD = r.minD;
            maxZ = std::move(r.maxZ);
            accumZsq = r.accumZsq;
            accumZmax = std::move(r.accumZmax);
            return *this;
        }

        /// Comparison operator
        bool operator==(const MDZCluster & r) const {
            return value == r.value
                   && minM == r.minM && minD == r.minD
                   && accumZsq == r.accumZsq && maxZ == r.maxZ && accumZmax == r.accumZmax;
        }

        /// Distance operator for the clustering algorithm
        double distance(const MDZCluster & r, MDZCluster & sum) const;

        bool far(const MDZCluster & r) const;

        /// Aggregation operator for the clustering algorithm
        void aggregate(const MDZCluster & r);

        /// Reduce the number of samples in the functions contained in this cluster
        void reduce();

        /// Check whether the functions of this cluster fulfill a certain request.
        bool fulfills(const TaskDescription & req) const {
            return minM.getValue() >= req.getMaxMemory() && minD.getValue() >= req.getMaxDisk();
        }

        uint32_t getValue() const {
            return value;
        }

        int32_t getTotalMemory() const {
            return minM.getValue() * value;
        }

        int32_t getTotalDisk() const {
            return minD.getValue() * value;
        }

        const ZAFunction & getMaximumSlowness() const {
            return maxZ;
        }

        /// Outputs a textual representation of the object.
        friend std::ostream & operator<<(std::ostream & os, const MDZCluster & o) {
            os << 'M' << o.minM << ',';
            os << 'D' << o.minD << ',';
            os << 'L' << o.maxZ << '-' << o.accumZsq << '-' << o.accumZmax << ',';
            return os << o.value;
        }

        MSGPACK_DEFINE(value, minM, minD, maxZ, accumZsq, accumZmax);

    private:
        friend class ClusteringList<MDZCluster>;
        friend class FSPAvailabilityInformation;

        FSPAvailabilityInformation * reference;

        uint32_t value;
        MinParameter<int32_t, int64_t> minM, minD;
        ZAFunction maxZ;
        double accumZsq;
        ZAFunction accumZmax;
    };

    MESSAGE_SUBCLASS(FSPAvailabilityInformation);

    FSPAvailabilityInformation() : AvailabilityInformation() { reset();}

    void reset() {
        summary.clear();
        memoryRange.setLimits(0);
        diskRange.setLimits(0);
        minZ = maxZ = ZAFunction();
        lengthHorizon = 0.0;
        slownessRange.setLimits(0.0);
        slownessSquareDiff = 0.0;
    }

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(cbrt(c));
    }

    const ClusteringList<MDZCluster> & getSummary() const {
        return summary;
    }

    bool operator==(const FSPAvailabilityInformation & r) const {
        return summary == r.summary && slownessRange == r.slownessRange;
    }

    /**
     * Obtain the maximum slowness reached when allocating a set of tasks of a certain application.
     * @param req Application requirements.
     */
    std::list<MDZCluster *> getFunctions(const TaskDescription & req);

    void removeClusters(const std::list<MDZCluster *> & clusters);

    void setAvailability(uint32_t m, uint32_t d, const FSPTaskList & curTasks, double power);

    /**
     * Returns the current minimum stretch for this set of nodes
     */
    double getMinimumSlowness() const {
        return slownessRange.getMin();
    }

    /**
     * Manually set the minimum and maximum stretch, at the routing nodes.
     */
    void setMinimumSlowness(double min) {
        slownessRange.setMinimum(min);
    }

    /**
     * Returns the current minimum stretch for this set of nodes
     */
    double getMaximumSlowness() const {
        return slownessRange.getMax();
    }

    /**
     * Manually set the minimum and maximum stretch, at the routing nodes.
     */
    void setMaximumSlowness(double max) {
        slownessRange.setMaximum(max);
    }

    double getSlowestMachine() const;

    /**
     * Aggregates an ExecutionNodeInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const FSPAvailabilityInformation & r);

    // This is documented in AvailabilityInformation.
    virtual void reduce();

    // This is documented in BasicMsg
    virtual void output(std::ostream& os) const;

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, memoryRange, diskRange, minZ, maxZ, lengthHorizon, slownessRange);

private:
    FSPAvailabilityInformation & operator=(const FSPAvailabilityInformation & copy) { return *this; }

    static unsigned int numClusters;
    static unsigned int numIntervals;

    ClusteringList<MDZCluster> summary;   ///< List of clusters representing queues and their availability
    Interval<int32_t> memoryRange;
    Interval<int32_t> diskRange;
    ZAFunction minZ, maxZ;                ///< Minimum and maximum values of availability
    double lengthHorizon;                 ///< Last meaningful task length
    Interval<double> slownessRange;   /// Slowness among the nodes in this branch
    double slownessSquareDiff;
};

} // namespace stars

#endif /* FSPAVAILABILITYINFORMATION_H_ */

