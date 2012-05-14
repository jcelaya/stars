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

#ifndef TIMECONSTRAINTINFO_H_
#define TIMECONSTRAINTINFO_H_

#include <vector>
#include <utility>
#include <ostream>
#include "Time.hpp"
#include "AvailabilityInformation.hpp"
#include "TaskDescription.hpp"
#include "ClusteringVector.hpp"


/**
 * \brief Availability information class with time constraints.
 *
 * This class describes the properties of a set of execution nodes, so that this information
 * may be aggregated and used in the search algorithm.
 */
class TimeConstraintInfo : public AvailabilityInformation {
    /// Set the basic elements for a Serializable descendant
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(AvailabilityInformation) & summary & minM & maxM & minD & maxD & minA & maxA & horizon;
        if (IS_LOADING())
            for (unsigned int i = 0; i < summary.getSize(); i++)
                summary[i].setReference(this);
    }

public:

    /**
     * \brief A cluster of functions a(t).
     *
     * This class describes a cluster of functions a(t), as a conservative approximation defined by linear segments.
     * The approximation must be non-decreasing, but it is not checked.
     * Segments are defined by start and end points, being the end of a segment the start of the next one.
     * The first point must have a value of zero, and functions are constant after the last point, called horizon.
     * The area under the function between the first point and the horizon is also precalculated and stored.
     *
     * Each instance contains a vector of points, with time and availability; a start time, when the value of the function
     * is zero, and thus not needed to record; and the precalculated area under the function between the start time and the
     * last recorded time. If the vector of points is empty, then the interpretation is different. The precalculated area
     * is that of one second after the current time.
     */
    class ATFunction {
        SRLZ_API SRLZ_METHOD() {
            // The functions are reduced when they are going to be sent, not before, to avoid loosing too much precision.
            //reduce();
            ar & points & slope;
        }

        /// Function points defining segments
        std::vector<std::pair<Time, uint64_t> > points;
        double slope;   ///< Slope at the end of the function

        // Steps through a vector of functions, with all their slope-change points, and the points where the two first functions cross
        template<int numF, class S> static void stepper(const ATFunction * (&f)[numF],
                const Time & ref, const Time & h, S & step);

    public:
        /// Default constructor
        ATFunction() : slope(0.0) {}

        /**
         * Creates an ATFunction from a slope value and a set of points. The curve is supposed to be a continuous
         * piecewise linear function, alternating a growing linear function with slope power and a function of
         * constant value. The list of points define the limits of each piece of the function. After the last point,
         * the function remains constant forever. There MUST be an even number of points.
         */
        ATFunction(double power, const std::list<Time> & p);

        /**
         * Returns the last slope of this function
         */
        double getSlope() const {
            return slope;
        }

        /**
         * Creates a function from the aggregation of two other. The result is a conservative approximation of the
         * sum of functions.
         * @param l The left function.
         * @param r The right function.
         */
        void min(const ATFunction & l, const ATFunction & r);

        /**
         * Creates a function from the aggregation of two other. The result is an optimistic approximation of the
         * sum of functions.
         * @param l The left function.
         * @param r The right function.
         */
        void max(const ATFunction & l, const ATFunction & r);

        /**
         * Calculates the squared difference with another function
         * @param r The other function
         * @param ref the initial time
         * @param h The horizon of measurement
         */
        double sqdiff(const ATFunction & r, const Time & ref, const Time & h) const;

        /**
         * Calculates the loss of the approximation to another function, with the least squares method, and the minimum
         * of two functions at the same time
         */
        double minAndLoss(const ATFunction & l, const ATFunction & r, unsigned int lv, unsigned int rv, const ATFunction & lc, const ATFunction & rc,
                          const Time & ref, const Time & h);

        /**
         * Calculates the linear combination of functions f and coefficients c
         * @param f Array of functions
         * @param c Array of function coefficients
         */
        template <int numF> void lc(const ATFunction * (&f)[numF], double(&c)[numF]);

        /**
         * Reduces the number of points of the function to a specific number, resulting in a function
         * with lower or equal value to the original.
         * @param v Number of nodes it represents
         * @param c Maximum function among the clustered ones.
         * @param ref the initial time
         * @param h The horizon of measurement
         */
        double reduceMin(unsigned int v, ATFunction & c, const Time & ref, const Time & h, unsigned int quality = 10);

        /**
         * Reduces the number of points of the function to a specific number, resulting in a function
         * with greater or equal value to the original.
         * Unlike the previous method, this one assumes that v = 1 and c is the null function.
         * @param ref the initial time
         * @param h The horizon of measurement
         */
        double reduceMax(const Time & ref, const Time & h, unsigned int quality = 10);

        /// Comparison operator
        bool operator==(const ATFunction & r) const {
            return slope == r.slope && points == r.points;
        }

        /// Transfers the values of f to this function, actually destroying f
        void transfer(ATFunction & f) {
            points.swap(f.points);
            slope = f.slope;
        }

        /// Return whether this is a free function
        bool isFree() const {
            return points.empty();
        }

        /**
         * Returns the time of the last point.
         */
        Time getHorizon() const {
            return points.empty() ? Time::getCurrentTime() + Duration(1.0) : points.back().first;
        }

        /**
         * Returns the available computation before a certain deadline.
         */
        uint64_t getAvailabilityBefore(Time d) const;

        /**
         * Reduces the availability when assigning a task with certain length and deadline
         */
        void update(uint64_t length, Time deadline, Time horizon);

        const std::vector<std::pair<Time, uint64_t> > & getPoints() const {
            return points;
        }

        friend std::ostream & operator<<(std::ostream & os, const ATFunction & o) {
            for (std::vector<std::pair<Time, uint64_t> >::const_iterator i = o.points.begin(); i != o.points.end(); i++)
                os << '(' << i->first << ',' << i->second << "),";
            return os << o.slope;
        }
    };

    /**
     * \brief A cluster of availability function with time constraints
     *
     * This cluster contains an aggregation of availability functions with memory, disk and time constraints.
     * The time constraint is expressed as a list of pairs, with a time value and the availability until that time.
     * The availability is considered constant after the last point
     */
    class MDFCluster {
        SRLZ_API SRLZ_METHOD() {
            ar & value & minM & accumMsq & accumMln & minD & accumDsq & accumDln & minA & accumAsq & accumMaxA;
        }

        TimeConstraintInfo * reference;

    public:

        uint32_t value;
        uint32_t minM, minD;
        ATFunction minA;
        uint64_t accumMsq, accumDsq, accumMln, accumDln;
        double accumAsq;
        ATFunction accumMaxA;

        /// Default constructor, for serialization purposes mainly
        MDFCluster() {}
        /// Creates a cluster for a certain information object r and a set of initial values
        MDFCluster(TimeConstraintInfo * r, uint32_t m, uint32_t d, double power, const std::list<Time> & p)
                : reference(r), value(1), minM(m), minD(d), minA(power, p), accumMsq(0), accumDsq(0),
                accumMln(0), accumDln(0), accumAsq(0.0), accumMaxA(power, p) {}

        /// Sets the information this cluster makes reference to.
        void setReference(TimeConstraintInfo * r) {
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

//  uint32_t getLostMemory(const TaskDescription & req) const { return minM - req.getMaxMemory(); }
//
//  uint32_t getLostDisk(const TaskDescription & req) const { return minD - req.getMaxDisk(); }

//  uint32_t getLostTime(const TaskDescription & req) const {
//   unsigned int max = (unsigned long)((req.getDeadline() - maxT).seconds()) % (req.getLength() / minP);
//   return (req.getDeadline() - maxT).seconds() - max * req.getLength() / minP;
//  }

        /// Outputs a textual representation of the object.
        friend std::ostream & operator<<(std::ostream & os, const MDFCluster & o) {
            os << 'M' << o.minM << '-' << o.accumMsq << '-' << o.accumMln << ',';
            os << 'D' << o.minD << '-' << o.accumDsq << '-' << o.accumDln << ',';
            os << 'A' << o.minA << '-' << o.accumAsq << '-' << o.accumMaxA << ',';
            return os << o.value;
        }
    };

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    static unsigned int numRefPoints;

    ClusteringVector<MDFCluster> summary;   ///< List of clusters representing queues and their availability
    uint32_t minM, maxM, minD, maxD;        ///< Minimum and maximum values of memory and disk availability
    ATFunction minA, maxA;                  ///< Minimum and maximum values of availability
    Time horizon;             ///< Last meaningful time

    // Aggregation variables
    unsigned int memRange, diskRange;
    double availRange;
    Time aggregationTime;     ///< Time at which aggregation is performed

public:
    class AssignmentInfo {
        AssignmentInfo(unsigned int c, uint32_t v, uint32_t m, uint32_t d, uint32_t t) :
                cluster(c), remngMem(m), remngDisk(d), remngAvail(t), numTasks(v) {}
        friend class TimeConstraintInfo;
    public:
        unsigned int cluster;
        uint32_t remngMem, remngDisk, remngAvail;
        uint32_t numTasks;
    };

    /// Default constructor, creates an empty information piece
    TimeConstraintInfo() {
        reset();
    }

    /// Copy constructor, sets the reference to the newly created object
    TimeConstraintInfo(const TimeConstraintInfo & copy) : AvailabilityInformation(copy), summary(copy.summary), minM(copy.minM),
            maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD), minA(copy.minA), maxA(copy.maxA), horizon(copy.horizon) {
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].setReference(this);
    }

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(cbrt(c));
    }

    static void setNumRefPoints(unsigned int n) {
        numRefPoints = n;
    }

    // This is documented in BasicMsg
    virtual TimeConstraintInfo * clone() const {
        return new TimeConstraintInfo(*this);
    }

    /// Clears the instance properties
    void reset() {
        summary.clear();
        minM = minD = maxM = maxD = 0;
        minA = maxA = ATFunction();
        horizon = Time::getCurrentTime();
    }

    /**
     * Initialize the cluster lists with one node
     * @param mem Available memory.
     * @param disk Available disk space.
     * @param power Computing power of the node
     * @param p List of points defining the curve of availability. See ATFunction.
     */
    void addNode(uint32_t mem, uint32_t disk, double power, const std::list<Time> & p);

    /**
     * Aggregates an TimeConstraintInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const TimeConstraintInfo & o);

    void reduce();

    // This is documented in AvailabilityInformation.h
    bool operator==(const TimeConstraintInfo & r) const {
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

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("TimeConstraintInfo");
    }

    const ClusteringVector<MDFCluster> & getSummary() const {
        return summary;
    }
};

#endif /*TIMECONSTRAINTINFO_H_*/
