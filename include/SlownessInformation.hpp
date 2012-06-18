/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
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

#ifndef SLOWNESSINFORMATION_H_
#define SLOWNESSINFORMATION_H_

#include <cmath>
#include <vector>
#include <list>
#include <utility>
#include <ostream>
#include <boost/shared_ptr.hpp>
#include "AvailabilityInformation.hpp"
#include "ClusteringVector.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
class Task;


/**
 * \brief Information about how slowness changes when a new application arrives.
 *
 * This class provides information about how the stretch in a certain set of nodes changes
 * when tasks of a new application are assigned to them.
 */
class SlownessInformation: public AvailabilityInformation {
public:
    /**
     * \brief Function L(a).
     *
     * This class describes function L(a), as an approximation defined by intervals.
     */
    class LAFunction {
    public:
        /**
         * \brief A piece of the h(S,w) function.
         *
         * This class provides the value of the function in one of its intervals.
         */
        class SubFunction {
        public:
            SubFunction() : x(0.0), y(0.0), z1(0.0), z2(0.0) {}

            SubFunction(double _x, double _y, double _z1, double _z2) : x(_x), y(_y), z1(_z1), z2(_z2) {}

            double value(double a, int n = 1) const {
                return x / a + y * a * n + z1 * n + z2;
            }

            bool operator==(const SubFunction & r) const {
                return x == r.x && y == r.y && z1 == r.z1 && z2 == r.z2;
            }

            bool operator!=(const SubFunction & r) const {
                return !operator==(r);
            }

            friend std::ostream & operator<<(std::ostream & os, const SubFunction & o) {
                return os << "L = " << o.x << "/a + " << o.y << "a + " << o.z1 << " + " << o.z2;
            }

            MSGPACK_DEFINE(x, y, z1, z2);

            // z1 is the sum of the independent term in L = x/a + z1, while z2 is the independent part in the other functions
            double x, y, z1, z2;   // L = x/a + y*a + z1 + z2
        };

        static const unsigned int minTaskLength = 1000;

        /// Default constructor
        LAFunction() {
            pieces.push_back(std::make_pair(minTaskLength, SubFunction()));
        }

        /**
         * Creates an LAFunction from a task queue.
         */
        LAFunction(const std::list<boost::shared_ptr<Task> > & tasks, double power);

        /**
         * Change the r_k reference time for this function.
         */
        void modifyReference(Time oldRef, Time newRef);

        /**
         * The minimum of two functions.
         * @param l The left function.
         * @param r The right function.
         */
        void min(const LAFunction & l, const LAFunction & r);

        /**
         * The maximum of two functions.
         * @param l The left function.
         * @param r The right function.
         */
        void max(const LAFunction & l, const LAFunction & r);

        /**
         * The sum of the differences between two functions and the maximum of them
         * @param l The left function.
         * @param r The right function.
         */
        void maxDiff(const LAFunction & l, const LAFunction & r, unsigned int lv, unsigned int rv,
                     const LAFunction & maxL, const LAFunction & maxR);

        /**
         * Calculates the squared difference with another function
         * @param r The other function.
         * @param sh The measurement horizon for the stretch.
         * @param wh The measurement horizon for the application length.
         */
        double sqdiff(const LAFunction & r, double ah) const;

        /**
         * Calculates the loss of the approximation to another function, with the least squares method, and the mean
         * of two functions at the same time
         */
        double maxAndLoss(const LAFunction & l, const LAFunction & r, unsigned int lv, unsigned int rv,
                          const LAFunction & maxL, const LAFunction & maxR, double ah);

        /**
         * Reduces the number of points of the function to a specific number, resulting in a function
         * with approximate value over the original.
         */
        double reduceMax(unsigned int v, double ah, unsigned int quality = 10);

        /// Comparison operator
        bool operator==(const LAFunction & r) const {
            return pieces == r.pieces;
        }

        /// Transfers the values of f to this function, actually destroying f
        void swap(LAFunction & f) {
            pieces.swap(f.pieces);
        }

        /// Returns the maximum significant task length
        double getHorizon() const {
            return pieces.empty() ? 0.0 : pieces.back().first;
        }

        const std::vector<std::pair<double, SubFunction> > & getPieces() const {
            return pieces;
        }

        /**
         * Returns the slowness reached for a certain application type.
         */
        double getSlowness(uint64_t a) const;

        /**
         * Estimates the slowness of allocating n tasks of length a
         */
        double estimateSlowness(uint64_t a, unsigned int n) const;

        /**
         * Reduces the availability when assigning a number of tasks with certain length
         */
        void update(uint64_t length, unsigned int n);

        /// Return the maximum among z1 values
        double getSlowestMachine() const;

        friend std::ostream & operator<<(std::ostream & os, const LAFunction & o) {
            os << "[LAF";
            for (std::vector<std::pair<double, SubFunction> >::const_iterator i = o.pieces.begin(); i != o.pieces.end(); i++) {
                os << " (" << i->first << ", " << i->second << ')';
            }
            return os << ']';
        }

        MSGPACK_DEFINE(pieces);
    private:
        // Steps through all the intervals of a pair of functions
        template<int numF, class S> static void stepper(const LAFunction * (&f)[numF], S & step);

        // Steppers for the stepper method
        struct minStep;
        struct maxStep;
        struct sqdiffStep;
        struct maxDiffStep;
        struct maxAndLossStep;

        /// Piece set
        std::vector<std::pair<double, SubFunction> > pieces;
    };

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
        MDLCluster(SlownessInformation * r, uint32_t m, uint32_t d, const std::list<boost::shared_ptr<Task> > & tasks, double power)
                : reference(r), value(1), minM(m), minD(d), maxL(tasks, power), accumMsq(0), accumDsq(0),
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

        SlownessInformation * reference;

        uint32_t value;
        uint32_t minM, minD;
        LAFunction maxL;
        uint64_t accumMsq, accumDsq, accumMln, accumDln;
        double accumLsq;
        LAFunction accumMaxL;
    };

    MESSAGE_SUBCLASS(SlownessInformation);
    
    /// Default constructor.
    SlownessInformation() : AvailabilityInformation(), minM(0), maxM(0), minD(0), maxD(0),
            lengthHorizon(0.0), minimumSlowness(0.0), maximumSlowness(0.0) {}

    /// Copy constructor, sets the reference to the newly created object.
    SlownessInformation(const SlownessInformation & copy) : AvailabilityInformation(copy), summary(copy.summary), minM(copy.minM),
            maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD), minL(copy.minL), maxL(copy.maxL), lengthHorizon(copy.lengthHorizon),
            minimumSlowness(copy.minimumSlowness), maximumSlowness(copy.maximumSlowness), rkref(copy.rkref) {
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].reference = this;
    }

    /// Sets the number of clusters allowed in the vector.
    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(cbrt(c));
    }

    /// Sets the number of reference points in each function.
    static void setNumPieces(unsigned int n) {
        numPieces = n;
    }

    const ClusteringVector<MDLCluster> & getSummary() const {
        return summary;
    }

    /**
     * Comparison operator.
     */
    bool operator==(const SlownessInformation & r) const {
        return summary == r.summary;
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
    void setAvailability(uint32_t m, uint32_t d, const std::list<boost::shared_ptr<Task> > & tasks, double power, double minSlowness);

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

    /**
     * Returns the time reference for the r_k parameter of the new potential application.
     * This reference is the same for every function in the summary.
     */
    Time getRkReference() const {
        return rkref;
    }

    void updateRkReference(Time newRef);

    double getSlowestMachine() const;

    /**
     * Aggregates an ExecutionNodeInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const SlownessInformation & r);

    // This is documented in AvailabilityInformation.
    virtual void reduce();

    // This is documented in BasicMsg
    virtual void output(std::ostream& os) const;

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, minM, maxM, minD, maxD, minL, maxL, lengthHorizon, minimumSlowness, maximumSlowness, rkref);
private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    static unsigned int numPieces;
    static const double infinity;

    ClusteringVector<MDLCluster> summary;   ///< List of clusters representing queues and their availability
    uint32_t minM, maxM, minD, maxD;        ///< Minimum and maximum values of memory and disk availability
    LAFunction minL, maxL;                  ///< Minimum and maximum values of availability
    double lengthHorizon;                   ///< Last meaningful task length
    double minimumSlowness;                  ///< Minimum slowness among the nodes in this branch
    double maximumSlowness;                  ///< Minimum slowness among the nodes in this branch
    /// Reference time for the r_k parameter in all the functions of this summary
    Time rkref;

    // Aggregation variables
    unsigned int memRange, diskRange;
    double slownessRange;
};

#endif /* SLOWNESSINFORMATION_H_ */

