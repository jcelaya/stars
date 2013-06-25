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

#ifndef MSPAVAILABILITYINFORMATION_H_
#define MSPAVAILABILITYINFORMATION_H_

#include <cmath>
#include <vector>
#include <list>
#include <utility>
#include <ostream>
#include <boost/shared_ptr.hpp>
#include "AvailabilityInformation.hpp"
#include "ClusteringList.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
#include "FSPTaskList.hpp"
#include "LAFunction.hpp"
#include "ScalarParameter.hpp"

namespace stars {

/**
 * \brief Information about how slowness changes when a new application arrives.
 *
 * This class provides information about how the stretch in a certain set of nodes changes
 * when tasks of a new application are assigned to them.
 */
class MSPAvailabilityInformation : public AvailabilityInformation {
public:
    /**
     * \brief A cluster of availability function with fair allocation constraints
     *
     * This cluster contains an aggregation of availability functions with memory, disk and fair allocation constraints.
     */
    class MDLCluster {
    public:
        /// Default constructor, for serialization purposes mainly
        MDLCluster() {}
        /// Creates a cluster for a certain information object r and a set of initial values
        MDLCluster(uint32_t m, uint32_t d, const FSPTaskList & curTasks, double power)
                : reference(NULL), value(1), minM(m), minD(d), maxL(curTasks, power), accumLsq(0.0), accumMaxL(maxL) {}

        /// Comparison operator
        bool operator==(const MDLCluster & r) const {
            return value == r.value
                   && minM == r.minM && minD == r.minD
                   && accumLsq == r.accumLsq && maxL == r.maxL && accumMaxL == r.accumMaxL;
        }

        /// Distance operator for the clustering algorithm
        double distance(const MDLCluster & r, MDLCluster & sum) const;

        bool far(const MDLCluster & r) const;

        /// Aggregation operator for the clustering algorithm
        void aggregate(const MDLCluster & r);

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

        const LAFunction & getMaximumSlowness() const {
            return maxL;
        }

        /// Outputs a textual representation of the object.
        friend std::ostream & operator<<(std::ostream & os, const MDLCluster & o) {
            os << 'M' << o.minM << ',';
            os << 'D' << o.minD << ',';
            os << 'L' << o.maxL << '-' << o.accumLsq << '-' << o.accumMaxL << ',';
            return os << o.value;
        }

        MSGPACK_DEFINE(value, minM, minD, maxL, accumLsq, accumMaxL);

    private:
        friend class ClusteringList<MDLCluster>;
        friend class MSPAvailabilityInformation;

        MSPAvailabilityInformation * reference;

        uint32_t value;
        MinParameter<int32_t, int64_t> minM, minD;
        LAFunction maxL;
        double accumLsq;
        LAFunction accumMaxL;
    };

    MESSAGE_SUBCLASS(MSPAvailabilityInformation);

    MSPAvailabilityInformation() : AvailabilityInformation(), memoryRange(0), diskRange(0),
            lengthHorizon(0.0), slownessRange(0.0) {}

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(cbrt(c));
    }

    const ClusteringList<MDLCluster> & getSummary() const {
        return summary;
    }

    bool operator==(const MSPAvailabilityInformation & r) const {
        return summary == r.summary && slownessRange == r.slownessRange;
    }

    /**
     * Obtain the maximum slowness reached when allocating a set of tasks of a certain application.
     * @param req Application requirements.
     */
    void getFunctions(const TaskDescription & req, std::vector<std::pair<LAFunction *, unsigned int> > & f);

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
    void join(const MSPAvailabilityInformation & r);

    // This is documented in AvailabilityInformation.
    virtual void reduce();

    // This is documented in BasicMsg
    virtual void output(std::ostream& os) const;

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, memoryRange, diskRange, minL, maxL, lengthHorizon, slownessRange);

private:
    MSPAvailabilityInformation & operator=(const MSPAvailabilityInformation & copy) { return *this; }

    static unsigned int numClusters;
    static unsigned int numIntervals;

    ClusteringList<MDLCluster> summary;   ///< List of clusters representing queues and their availability
    Interval<int32_t> memoryRange;
    Interval<int32_t> diskRange;
    LAFunction minL, maxL;                ///< Minimum and maximum values of availability
    double lengthHorizon;                 ///< Last meaningful task length
    Interval<double> slownessRange;   /// Slowness among the nodes in this branch
    double slownessSquareDiff;
};

} // namespace stars

#endif /* MSPAVAILABILITYINFORMATION_H_ */

