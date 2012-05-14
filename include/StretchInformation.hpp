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

#ifndef STRETCHINFORMATION_H_
#define STRETCHINFORMATION_H_

#include <cmath>
#include <vector>
#include <list>
#include <utility>
#include <ostream>
#include <boost/shared_ptr.hpp>
#include "AvailabilityInformation.hpp"
#include "ClusteringVector.hpp"
#include "Time.hpp"
class Task;


/**
 * \brief Information about how stretch changes when a new application arrives.
 *
 * This class provides information about how the stretch in a certain set of nodes changes
 * when tasks of a new application are assigned to them.
 */
class StretchInformation: public AvailabilityInformation {
public:
    struct AppDesc {
        typedef std::list<boost::shared_ptr<Task> >::const_iterator taskIterator;

        double w, r, a, d, asum;
        taskIterator firstTask, endTask;
        AppDesc(taskIterator start, taskIterator end, Time ref) : w((*start)->getDescription().getAppLength()),
                r(((*start)->getCreationTime() - ref).seconds()), a(0.0), d(0.0), asum(0.0), firstTask(start), endTask(end) {
            for (taskIterator it = start; it != end; ++it)
                a += (*it)->getEstimatedDuration().seconds();
        }
        double getDeadline(double S) const {
            return S * w + r;
        }
        void setStretch(double S) {
            d = getDeadline(S);
        }
        bool operator<(const AppDesc & rapp) const {
            return d < rapp.d || (d == rapp.d && w < rapp.w);
        }
        friend std::ostream & operator<<(std::ostream & os, const AppDesc & o) {
            return os << "w=" << o.w << " r=" << o.r << " a=" << o.a;
        }
    };

    /**
     * \brief Function h(S,w).
     *
     * This class describes function h(S,w), as an approximation defined by bidimensional intervals.
     */
    class HSWFunction {
    public:
        /**
         * \brief A piece of the h(S,w) function.
         *
         * This class provides the value of the function in one of its intervals.
         */
        class SubFunction {
        public:
            SubFunction() : a(0.0), b(0.0), c(0.0) {}

            SubFunction(double _a, double _b, double _c) : a(_a), b(_b), c(_c) {}

            double value(double S, double w) const {
                return S * (w * a + b) - c;
            }

            bool operator==(const SubFunction & r) const {
                return a == r.a && b == r.b && c == r.c;
            }

            friend std::ostream & operator<<(std::ostream & os, const SubFunction & o) {
                return os << "h = S(" << o.a << "w + " << o.b << ") - " << o.c;
            }

            SRLZ_API SRLZ_METHOD() {
                ar & a & b & c;
            }

            double a, b, c;   // h = S(wa + b) - c
        };

        /**
         * \brief A piece of the piecewise h(S,w) function.
         */
        class Piece {
        public:
            /// Default constructor
            Piece() : s(0.0), d(0.0), e(0.0), ps(-1), pw(-1), ns(-1), nw(-1) {}

            /// Constructor with full information.
            Piece(double startS, double bottomD, double bottomE, const SubFunction & expr) :
                    f(expr), s(startS), d(bottomD), e(bottomE), ps(-1), pw(-1), ns(-1), nw(-1) {}

            double w(double S) const {
                return d / S + e;
            }

            bool operator==(const Piece & r) const {
                return s == r.s && d == r.d && e == r.e && f == r.f;
            }

            bool operator<(const Piece & r) const {
                return s < r.s || (s == r.s && d / s + e < r.d / s + r.e);
            }

            bool isInRange(const std::vector<Piece> & b, double Si, double wi) const {
                return Si >= s && (ns < 0 || Si < b[ns].s) && wi >= w(Si) && (nw < 0 || wi < b[nw].w(Si));
            }

            void intersection(const std::vector<Piece> & b, const Piece & r, Piece * result, int & numConstructed) const;

            void output(const std::vector<Piece> & b, std::ostream & os) const {
                os << '[' << s << ", ";
                if (ns < 0) os << "inf";
                else os << b[ns].s;
                os << ") : [" << d << "/S + " << e << ", ";
                if (nw < 0) os << "inf)";
                else os << b[nw].d << "/S + " << b[nw].e << ")";
                os << " ; " << f;
            }

            SRLZ_API SRLZ_METHOD() {
                ar & f & s & d & e & ps & pw & ns & nw;
            }

            SubFunction f;   /// The expression of this piece of the function.
            double s;        /// Lower bound for the S parameter, S >= s in this piece.
            double d, e;     /// Lower bound for the w parameter, w >= d/s + e in this piece.
            int ps;          /// The previous piece in the S parameter dimension.
            int pw;          /// The previous piece in the w parameter dimension.
            int ns;          /// The next piece in the S parameter dimension.
            int nw;          /// The next piece in the w parameter dimension.
        };

        /// Default constructor
        HSWFunction() : minStretch(0.0) {}

        /**
         * Creates an HSWFunction from a task queue.
         */
        HSWFunction(std::list<AppDesc> & apps, double power);

        const std::vector<Piece> & getPieces() const {
            return pieces;
        }

        /**
         * The minimum of two functions.
         * @param l The left function.
         * @param r The right function.
         */
        void min(const HSWFunction & l, const HSWFunction & r);

        /**
         * The maximum of two functions.
         * @param l The left function.
         * @param r The right function.
         */
        void max(const HSWFunction & l, const HSWFunction & r);

        /**
         * Calculates the squared difference with another function
         * @param r The other function.
         * @param sh The measurement horizon for the stretch.
         * @param wh The measurement horizon for the application length.
         */
        double sqdiff(const HSWFunction & r, double sh, double wh) const;

        /**
         * Calculates the loss of the approximation to another function, with the least squares method, and the mean
         * of two functions at the same time
         */
        double meanAndLoss(const HSWFunction & l, const HSWFunction & r, unsigned int lv, unsigned int rv, double sh, double wh);

        /**
         * Reduces the number of points of the function to a specific number, resulting in a function
         * with approximate value to the original.
         */
        double reduce(double sh, double wh, unsigned int quality = 10);

        /// Comparison operator
        bool operator==(const HSWFunction & r) const {
            return minStretch == r.minStretch && pieces == r.pieces;
        }

        /// Transfers the values of f to this function, actually destroying f
        void swap(HSWFunction & f) {
            minStretch = f.minStretch;
            pieces.swap(f.pieces);
        }

        /// Returns the minimum significant stretch
        double getMinStretch() const {
            return minStretch;
        }

        /// Returns the maximum significant stretch and application length
        void getHorizon(double & sh, double & wh) const {
            sh = 0.0;
            // Get the piece with highest stretch
            for (int p = 0; p >= 0; p = pieces[p].ns >= 0 ? pieces[p].ns : pieces[p].nw)
                if (pieces[p].s > sh)
                    sh = pieces[p].s;
            wh = 0.0;
            // Get the piece with highest w at the stretch horizon
            for (int p = 0; p >= 0; p = pieces[p].nw >= 0 ? pieces[p].nw : pieces[p].ns)
                if (pieces[p].d / sh + pieces[p].e > wh)
                    wh = pieces[p].d / sh + pieces[p].e;
        }

        /**
         * Returns the available computation for a certain application type.
         */
        uint64_t getAvailability(double S, double w) const;

        /**
         * Reduces the availability when assigning a task with certain length and deadline
         */
//  void update(uint64_t length, Time deadline, Time horizon);

        friend std::ostream & operator<<(std::ostream & os, const HSWFunction & o) {
            os << "HSWF";
            for (std::vector<Piece>::const_iterator i = o.pieces.begin(); i != o.pieces.end(); i++) {
                os << ", (";
                i->output(o.pieces, os);
                os << ')';
            }
            return os;
        }

    private:
        // Steppers
        struct minStep;
        struct maxStep;
        struct sqdiffStep;
        struct meanLossStep;
        //struct IntervalCluster;
        struct ReduceOption;

        /**
         * Insert a piece in a set of pieces. The new piece is expected to be next to other two pieces,
         * which still have no lower and right neighbour.
         */
        static void insertNextTo(const Piece & p, int & lpos, int & upos, std::vector<Piece> & b);

        // Steps through all the intervals of a pair of functions
        template<class S> static void stepper(const HSWFunction & l, const HSWFunction & r, S & step);

        SRLZ_API SRLZ_METHOD() {
            ar & pieces & minStretch;
        }

        /// Piece set
        std::vector<Piece> pieces;
        double minStretch;
    };

    /**
     * This object represents the availability function for a specific application.
     * It returns the list of stretch values at which the number of tasks is increased by one.
     */
    class SpecificAF {
    public:
        /// Returns the stretch at the current step.
        double getCurrentStretch() const {
            return stretch;
        }

        /// Returns the number of tasks that can be added at each step.
        unsigned int getNumNodes() const {
            return numNodes;
        }

        /// Advances one step.
        void step() {
            ++k;
            while (calculateStretch() > it->first) ++it;
        }

    private:
        double calculateStretch() {
            return stretch = (k * a + it->second.c) / (w * it->second.a + it->second.b);
        }

        friend class StretchInformation;
        /// Constructor, creates the list of stretch values for a certain application parameters w and a.
        SpecificAF(HSWFunction & fi, unsigned int wi, unsigned int ai, unsigned int nodes);

        /// The list of functions which range is cut by plane w = wi.
        std::list<std::pair<double, const HSWFunction::SubFunction &> > functions;
        /// The current range in the list.
        std::list<std::pair<double, const HSWFunction::SubFunction &> >::iterator it;
        unsigned int k;          ///< The current step.
        double stretch;          ///< The current stretch;
        HSWFunction & func;      ///< The associated function.
        unsigned int w, a;       ///< The application properties
        unsigned int numNodes;   ///< The number of nodes represented in this function
    };

    /**
     * \brief A cluster of availability function with fair allocation constraints
     *
     * This cluster contains an aggregation of availability functions with memory, disk and fair allocation constraints.
     */
    class MDHCluster {
    public:
        /// Default constructor, for serialization purposes mainly
        MDHCluster() {}
        /// Creates a cluster for a certain information object r and a set of initial values
        MDHCluster(StretchInformation * r, uint32_t m, uint32_t d, std::list<AppDesc> & apps, double power)
                : reference(r), value(1), minM(m), minD(d), meanH(apps, power), accumMsq(0), accumDsq(0),
                accumMln(0), accumDln(0), accumHsq(0.0) {}

        /// Comparison operator
        bool operator==(const MDHCluster & r) const {
            return value == r.value
                   && minM == r.minM && accumMsq == r.accumMsq && accumMln == r.accumMln
                   && minD == r.minD && accumDsq == r.accumDsq && accumDln == r.accumDln
                   && accumHsq == r.accumHsq && meanH == r.meanH;
        }

        /// Distance operator for the clustering algorithm
        double distance(const MDHCluster & r, MDHCluster & sum) const;

        bool far(const MDHCluster & r) const;

        /// Aggregation operator for the clustering algorithm
        void aggregate(const MDHCluster & r);

        /// Constructs a cluster from the aggregation of two. It is mainly useful for the distance operator.
        void aggregate(const MDHCluster & l, const MDHCluster & r);

        /// Reduce the number of samples in the functions contained in this cluster
        //void reduce();

        /// Check whether the functions of this cluster fulfill a certain request.
        bool fulfills(const TaskDescription & req) const {
            return minM >= req.getMaxMemory() && minD >= req.getMaxDisk();
        }

        /// Outputs a textual representation of the object.
        friend std::ostream & operator<<(std::ostream & os, const MDHCluster & o) {
            os << 'M' << o.minM << '-' << o.accumMsq << '-' << o.accumMln << ',';
            os << 'D' << o.minD << '-' << o.accumDsq << '-' << o.accumDln << ',';
            os << 'A' << o.meanH << '-' << o.accumHsq << ',';
            return os << o.value;
        }

        SRLZ_API SRLZ_METHOD() {
            ar & value & minM & accumMsq & accumMln & minD & accumDsq & accumDln & meanH & accumHsq;
        }

        StretchInformation * reference;

        uint32_t value;
        uint32_t minM, minD;
        HSWFunction meanH;
        uint64_t accumMsq, accumDsq, accumMln, accumDln;
        double accumHsq;
    };

    /// Default constructor.
    StretchInformation() : AvailabilityInformation(), summary(), minM(0), maxM(0), minD(0), maxD(0), minH(), maxH(),
            stretchHorizon(0.0), lengthHorizon(0.0), minimumStretch(0.0), maximumStretch(0.0) {}

    /// Copy constructor, sets the reference to the newly created object.
    StretchInformation(const StretchInformation & copy) : AvailabilityInformation(copy), summary(copy.summary), minM(copy.minM),
            maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD), minH(copy.minH), maxH(copy.maxH), stretchHorizon(copy.stretchHorizon),
            lengthHorizon(copy.lengthHorizon), minimumStretch(copy.minimumStretch), maximumStretch(copy.maximumStretch) {
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

    const ClusteringVector<MDHCluster> & getSummary() const {
        return summary;
    }

    /**
     * Comparison operator.
     */
    bool operator==(const StretchInformation & r) const {
        return summary == r.summary;
    }

    /**
     * Obtain the number of tasks of a certain application that can be allocated with the given stretch.
     * @param req Application requirements.
     * @param stretch The target stretch.
     */
    unsigned int getAvailableSlots(const TaskDescription & req, double stretch);

    /**
     * Creates a list of SpecificAF objects from the functions that fulfill the provided requirements,
     * and appends it to the supplied one.
     * @param req Application requirements.
     * @param specificFunctions List of functions that is increased with the result.
     */
    void getSpecificFunctions(const TaskDescription & req, std::list<SpecificAF> & specificFunctions);

    /**
     * Aggregates an ExecutionNodeInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const StretchInformation & r);

    /**
     * Updates the values in the matrix, substracting a set of tasks from every cell.
     * @param n The number of tasks to be substracted.
     * @param a The size of the tasks, to adjust that number of tasks.
     */
    //void update(uint32_t n, uint64_t a);

    /**
     * Initializes the information in this summary from the data of the scheduler.
     * @param m Available memory in the execution node.
     * @param d Available disk space in the execution node.
     * @param tasks Task queue.
     * @param power Computing power of the execution node.
     */
    void setAvailability(uint32_t m, uint32_t d, std::list<AppDesc> & apps, double power);

    /**
     * Returns the current minimum stretch for this set of nodes
     */
    double getMinimumStretch() const {
        return minimumStretch;
    }

    /**
     * Returns the current maximum stretch for this set of nodes
     */
    double getMaximumStretch() const {
        return maximumStretch;
    }

    /**
     * Manually set the minimum and maximum stretch, at the routing nodes.
     */
    void setMinAndMaxStretch(double min, double max) {
        minimumStretch = min;
        maximumStretch = max;
    }

    // This is documented in AvailabilityInformation.
    virtual void reduce();

    // This is documented in BasicMsg.
    virtual StretchInformation * clone() const {
        return new StretchInformation(*this);
    }

    // This is documented in BasicMsg
    virtual void output(std::ostream& os) const;

    // This is documented in BasicMsg
    virtual std::string getName() const {
        return std::string("StretchInformation");
    }

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    static unsigned int numPieces;

    /// Set the basic elements for a Serializable class
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(AvailabilityInformation) & summary & minM & maxM & minD & maxD & minH & maxH
        & stretchHorizon & lengthHorizon & minimumStretch & maximumStretch;
        if (IS_LOADING())
            for (unsigned int i = 0; i < summary.getSize(); i++)
                summary[i].reference = this;
    }

    ClusteringVector<MDHCluster> summary;   ///< List of clusters representing queues and their availability
    uint32_t minM, maxM, minD, maxD;        ///< Minimum and maximum values of memory and disk availability
    HSWFunction minH, maxH;                 ///< Minimum and maximum values of availability
    double stretchHorizon;                  ///< Last meaningful stretch
    double lengthHorizon;                   ///< Last meaningful application length
    double minimumStretch;                  ///< Minimum stretch among the nodes in this branch
    double maximumStretch;                  ///< Maximum stretch among the nodes in this branch

    // Aggregation variables
    unsigned int memRange, diskRange;
    double availRange;
};

#endif /* STRETCHINFORMATION_H_ */
