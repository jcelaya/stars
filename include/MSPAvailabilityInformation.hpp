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
#include "TaskProxy.hpp"
#include "LAFunction.hpp"

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
        MDLCluster(MSPAvailabilityInformation * r, uint32_t m, uint32_t d, const TaskProxy::List & curTasks,
                const std::vector<double> & switchValues, double power)
                : reference(r), value(1), minM(m), minD(d), maxL(curTasks, switchValues, power), accumMsq(0), accumDsq(0),
                accumMln(0), accumDln(0), accumLsq(0.0), accumMaxL(maxL) {}

        /// Comparison operator
        bool operator==(const MDLCluster & r) const {
            return value == r.value
                   && minM == r.minM && accumMsq == r.accumMsq && accumMln == r.accumMln
                   && minD == r.minD && accumDsq == r.accumDsq && accumDln == r.accumDln
                   && accumLsq == r.accumLsq && maxL == r.maxL && accumMaxL == r.accumMaxL;
        }

        /// Distance operator for the clustering algorithm
        double distance(const MDLCluster & r, MDLCluster & sum) const;

        bool far(const MDLCluster & r) const;

        /// Aggregation operator for the clustering algorithm
        void aggregate(const MDLCluster & r);

        /// Constructs a cluster from the aggregation of two. It is mainly useful for the distance operator.
        void aggregate(const MDLCluster & l, const MDLCluster & r);

        /// Reduce the number of samples in the functions contained in this cluster
        void reduce();

        /// Check whether the functions of this cluster fulfill a certain request.
        bool fulfills(const TaskDescription & req) const {
            return minM >= req.getMaxMemory() && minD >= req.getMaxDisk();
        }

        /// Outputs a textual representation of the object.
        friend std::ostream & operator<<(std::ostream & os, const MDLCluster & o) {
            os << 'M' << o.minM << '-' << o.accumMsq << '-' << o.accumMln << ',';
            os << 'D' << o.minD << '-' << o.accumDsq << '-' << o.accumDln << ',';
            os << 'L' << o.maxL << '-' << o.accumLsq << '-' << o.accumMaxL << ',';
            return os << o.value;
        }

        MSGPACK_DEFINE(value, minM, accumMsq, accumMln, minD, accumDsq, accumDln, maxL, accumLsq, accumMaxL);

        MSPAvailabilityInformation * reference;

        uint32_t value;
        uint32_t minM, minD;
        LAFunction maxL;
        uint64_t accumMsq, accumDsq, accumMln, accumDln;
        double accumLsq;
        LAFunction accumMaxL;
    };

    MESSAGE_SUBCLASS(MSPAvailabilityInformation);

    /// Default constructor.
    MSPAvailabilityInformation() : AvailabilityInformation(), minM(0), maxM(0), minD(0), maxD(0),
            lengthHorizon(0.0), minimumSlowness(0.0), maximumSlowness(0.0) {}

    /// Sets the number of clusters allowed in the vector.
    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(cbrt(c));
    }

    const ClusteringList<MDLCluster> & getSummary() const {
        return summary;
    }

    /**
     * Comparison operator.
     */
    bool operator==(const MSPAvailabilityInformation & r) const {
        return summary == r.summary && minimumSlowness == r.minimumSlowness && maximumSlowness == r.maximumSlowness;
    }

    /**
     * Obtain the maximum slowness reached when allocating a set of tasks of a certain application.
     * @param req Application requirements.
     */
    void getFunctions(const TaskDescription & req, std::vector<std::pair<LAFunction *, unsigned int> > & f);

    /**
     * Initializes the information in this summary from the data of the scheduler.
     * @param m Available memory in the execution node.
     * @param d Available disk space in the execution node.
     * @param tasks Task queue.
     * @param power Computing power of the execution node.
     */
    void setAvailability(uint32_t m, uint32_t d, const TaskProxy::List & curTasks,
            const std::vector<double> & switchValues, double power, double minSlowness);

    /**
     * Returns the current minimum stretch for this set of nodes
     */
    double getMinimumSlowness() const {
        return minimumSlowness;
    }

    /**
     * Manually set the minimum and maximum stretch, at the routing nodes.
     */
    void setMinimumSlowness(double min) {
        minimumSlowness = min;
    }

    /**
     * Returns the current minimum stretch for this set of nodes
     */
    double getMaximumSlowness() const {
        return maximumSlowness;
    }

    /**
     * Manually set the minimum and maximum stretch, at the routing nodes.
     */
    void setMaximumSlowness(double max) {
        maximumSlowness = max;
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

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, minM, maxM, minD, maxD, minL, maxL, lengthHorizon, minimumSlowness, maximumSlowness);
private:
    /// Copy constructor, sets the reference to the newly created object.
    MSPAvailabilityInformation(const MSPAvailabilityInformation & copy) : AvailabilityInformation(copy), summary(copy.summary), minM(copy.minM),
            maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD), minL(copy.minL), maxL(copy.maxL), lengthHorizon(copy.lengthHorizon),
            minimumSlowness(copy.minimumSlowness), maximumSlowness(copy.maximumSlowness) {}

    MSPAvailabilityInformation & operator=(const MSPAvailabilityInformation & copy) {}

    static unsigned int numClusters;
    static unsigned int numIntervals;

    ClusteringList<MDLCluster> summary;   ///< List of clusters representing queues and their availability
    uint32_t minM, maxM, minD, maxD;        ///< Minimum and maximum values of memory and disk availability
    LAFunction minL, maxL;                  ///< Minimum and maximum values of availability
    double lengthHorizon;                   ///< Last meaningful task length
    double minimumSlowness;                 ///< Minimum slowness among the nodes in this branch
    double maximumSlowness;                 ///< Minimum slowness among the nodes in this branch

    // Aggregation variables
    unsigned int memRange, diskRange;
    double slownessRange;
};

} // namespace stars

#endif /* MSPAVAILABILITYINFORMATION_H_ */

