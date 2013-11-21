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

#ifndef DPAVAILABILITYINFORMATION_H_
#define DPAVAILABILITYINFORMATION_H_

#include <vector>
#include <utility>
#include <ostream>
#include "Time.hpp"
#include "AvailabilityInformation.hpp"
#include "TaskDescription.hpp"
#include "ClusteringList.hpp"
#include "LDeltaFunction.hpp"


/**
 * \brief Availability information class with time constraints.
 *
 * This class describes the properties of a set of execution nodes, so that this information
 * may be aggregated and used in the search algorithm.
 */
class DPAvailabilityInformation : public AvailabilityInformation {
public:

    /**
     * \brief A cluster of availability function with time constraints
     *
     * This cluster contains an aggregation of availability functions with memory, disk and time constraints.
     * The time constraint is expressed as a list of pairs, with a time value and the availability until that time.
     * The availability is considered constant after the last point
     */
    class MDFCluster {
    public:
        MSGPACK_DEFINE(value, minM, minD, minA, accumMsq, accumDsq, accumMln, accumDln, accumAsq, accumMaxA);

        uint32_t value;
        uint32_t minM, minD;
        stars::LDeltaFunction minA;
        double accumMsq, accumDsq, accumMln, accumDln;
        double accumAsq;
        stars::LDeltaFunction accumMaxA;

        /// Default constructor, for serialization purposes mainly
        MDFCluster() {}
        /// Creates a cluster for a certain information object r and a set of initial values
        MDFCluster(uint32_t m, uint32_t d, double power, const std::list<std::shared_ptr<Task> > & queue)
                : reference(NULL), value(1), minM(m), minD(d), minA(power, queue), accumMsq(0.0), accumDsq(0.0),
                accumMln(0.0), accumDln(0.0), accumAsq(0.0), accumMaxA(minA) {}

        /// Sets the information this cluster makes reference to.
        void setReference(DPAvailabilityInformation * r) {
            reference = r;
        }

        /// Comparison operator
        bool operator==(const MDFCluster & r) const {
            return value == r.value
                   && minM == r.minM && accumMsq == r.accumMsq && accumMln == r.accumMln
                   && minD == r.minD && accumDsq == r.accumDsq && accumDln == r.accumDln
                   && accumAsq == r.accumAsq && minA == r.minA && accumMaxA == r.accumMaxA;
        }

        /// Distance operator for the clustering algorithm
        double distance(const MDFCluster & r, MDFCluster & sum) const;

        bool far(const MDFCluster & r) const;

        /// Aggregation operator for the clustering algorithm
        void aggregate(const MDFCluster & r);

        /// Constructs a cluster from the aggregation of two. It is mainly useful for the distance operator.
        void aggregate(const MDFCluster & l, const MDFCluster & r);

        /// Reduce the number of samples in the functions contained in this cluster
        void reduce();

        /// Check whether the functions of this cluster fulfill a certain request.
        bool fulfills(const TaskDescription & req) const {
            return minM >= req.getMaxMemory() && minD >= req.getMaxDisk();
        }

        /// Outputs a textual representation of the object.
        friend std::ostream & operator<<(std::ostream & os, const MDFCluster & o) {
            os << 'M' << o.minM << '-' << o.accumMsq << '-' << o.accumMln << ',';
            os << 'D' << o.minD << '-' << o.accumDsq << '-' << o.accumDln << ',';
            os << 'A' << o.minA << '-' << o.accumAsq << '-' << o.accumMaxA << ',';
            return os << o.value;
        }

    private:
        DPAvailabilityInformation * reference;
    };

    class AssignmentInfo {
        AssignmentInfo(MDFCluster * c, uint32_t v, uint32_t m, uint32_t d, uint32_t t) :
                cluster(c), remngMem(m), remngDisk(d), remngAvail(t), numTasks(v) {}
        friend class DPAvailabilityInformation;
    public:
        MDFCluster * cluster;
        uint32_t remngMem, remngDisk, remngAvail;
        uint32_t numTasks;
    };

    MESSAGE_SUBCLASS(DPAvailabilityInformation);

    /// Default constructor, creates an empty information piece
    DPAvailabilityInformation() {
        reset();
    }

    /// Copy constructor, sets the reference to the newly created object
    DPAvailabilityInformation(const DPAvailabilityInformation & copy) : AvailabilityInformation(copy), summary(copy.summary), minM(copy.minM),
            maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD), minA(copy.minA), maxA(copy.maxA), horizon(copy.horizon) {
//        unsigned int size = summary.getSize();
//        for (unsigned int i = 0; i < size; i++)
//            summary[i].setReference(this);
    }

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(cbrt(c));
    }

    /// Clears the instance properties
    void reset() {
        summary.clear();
        minM = minD = maxM = maxD = 0;
        minA = maxA = stars::LDeltaFunction();
        horizon = Time::getCurrentTime();
    }

    void addNode(uint32_t mem, uint32_t disk, double power, const std::list<std::shared_ptr<Task> > & queue);

    /**
     * Aggregates an TimeConstraintInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const DPAvailabilityInformation & o);

    void reduce();

    // This is documented in AvailabilityInformation.h
    bool operator==(const DPAvailabilityInformation & r) const {
        return summary == r.summary;
    }

    /**
     * Returns the number of nodes that have at least enough computing power to finish a certain task.
     * @param desc The TaskDescription for a certain task.
     * @return The number of nodes with at least that availability.
     */
    void getAvailability(std::list<AssignmentInfo> & ai, const TaskDescription & desc) const;

    /**
     * Updates this information with the assignment of a set of tasks to certain clusters
     * @param ai List of AssignmentInfo with the number of tasks assigned to each cluster
     * @param desc Properties of the assigned tasks
     */
    void update(const std::list<AssignmentInfo> & ai, const TaskDescription & desc);

    // This is documented in BasicMsg
    void output(std::ostream& os) const;

    const stars::ClusteringList<MDFCluster> & getSummary() const {
        return summary;
    }

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, minM, maxM, minD, maxD, minA, maxA, horizon);
private:
    static unsigned int numClusters;
    static unsigned int numIntervals;

    stars::ClusteringList<MDFCluster> summary;   ///< List of clusters representing queues and their availability
    uint32_t minM, maxM, minD, maxD;        ///< Minimum and maximum values of memory and disk availability
    stars::LDeltaFunction minA, maxA;                  ///< Minimum and maximum values of availability
    Time horizon;             ///< Last meaningful time

    // Aggregation variables
    unsigned int memRange, diskRange;
    double availRange;
    Time aggregationTime;     ///< Time at which aggregation is performed
};

#endif /* DPAVAILABILITYINFORMATION_H_ */
