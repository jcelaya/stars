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

#ifndef QUEUEBALANCINGINFO_H_
#define QUEUEBALANCINGINFO_H_

#include <cmath>
#include "Time.hpp"
#include "AvailabilityInformation.hpp"
#include "ClusteringVector.hpp"
#include "TaskDescription.hpp"


/**
 * \brief Availability information class with queue length information.
 *
 * This class describes the properties of a set of execution nodes, so that this information
 * may be aggregated and used in the search algorithm.
 */
class QueueBalancingInfo : public AvailabilityInformation {
    /// Set the basic elements for a Serializable descendant
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(AvailabilityInformation) & minQueue & summary & minM & maxM & minD & maxD & minP & maxP & minT & maxT;
        if (IS_LOADING())
            for (unsigned int i = 0; i < summary.getSize(); i++)
                summary[i].setReference(this);
    }

public:
    class MDPTCluster {
        SRLZ_API SRLZ_METHOD() {
            ar & value & minM & accumM & minD & accumD & minP & accumP & maxT & accumT;
        }

        QueueBalancingInfo * reference;

    public:
        uint32_t value;
        uint32_t minM, minD, minP;
        uint64_t accumM, accumD, accumP;
        Time maxT;
        Duration accumT;

        MDPTCluster() : reference(NULL), value(0) {}
        MDPTCluster(QueueBalancingInfo * r, uint32_t m, uint32_t d, uint32_t p, Time t)
                : reference(r), value(1), minM(m), minD(d), minP(p), accumM(0), accumD(0), accumP(0), maxT(t), accumT(0.0) {}

        void setReference(QueueBalancingInfo * r) {
            reference = r;
        }

        // Order by maxT value
        bool operator<(const MDPTCluster & r) const {
            return maxT < r.maxT;
        }

        bool operator==(const MDPTCluster & r) const {
            return minM == r.minM && accumM == r.accumM && minD == r.minD && accumD == r.accumD
                   && minP == r.minP && accumP == r.accumP && maxT == r.maxT && accumT == r.accumT && value == r.value;
        }

        double distance(const MDPTCluster & r, MDPTCluster & sum) const;

        bool far(const MDPTCluster & r) const;

        void aggregate(const MDPTCluster & r);

        bool fulfills(const TaskDescription & req) const {
            return minM >= req.getMaxMemory() && minD >= req.getMaxDisk();
        }

        uint32_t getLostMemory(const TaskDescription & req) const {
            return minM - req.getMaxMemory();
        }

        uint32_t getLostDisk(const TaskDescription & req) const {
            return minD - req.getMaxDisk();
        }

        uint32_t getLostTime(const TaskDescription & req) const {
            unsigned int max = (unsigned long)((req.getDeadline() - maxT).seconds()) % (req.getLength() / minP);
            return (req.getDeadline() - maxT).seconds() - max * req.getLength() / minP;
        }

        static std::string getName() {
            return std::string("MDPTCluster");
        }

        friend std::ostream & operator<<(std::ostream & os, const MDPTCluster & o) {
            os << 'M' << o.minM << '-' << o.accumM << ',';
            os << 'D' << o.minD << '-' << o.accumD << ',';
            os << 'P' << o.minP << '-' << o.accumP << ',';
            os << 'T' << o.maxT << '-' << o.accumT << ',';
            return os << o.value;
        }
    };

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    Time minQueue;
    ClusteringVector<MDPTCluster> summary;   ///< List clusters representing queues
    uint32_t minM, maxM, minD, maxD, minP, maxP;
    Time minT, maxT;

public:
    QueueBalancingInfo() {
        reset();
    }
    QueueBalancingInfo(const QueueBalancingInfo & copy) : AvailabilityInformation(copy), minQueue(copy.minQueue), summary(copy.summary), minM(copy.minM),
            maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD), minP(copy.minP), maxP(copy.maxP), minT(copy.minT), maxT(copy.maxT) {
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].setReference(this);
    }

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(sqrt(c)));
    }

    void reset() {
        minQueue = Time::getCurrentTime();
        summary.clear();
        minM = maxM = minD = maxD = minP = maxP = 0;
        minT = maxT = minQueue;
    }

    // This is documented in BasicMsg
    virtual QueueBalancingInfo * clone() const {
        return new QueueBalancingInfo(*this);
    }

    /**
     * Add a cluster to the list
     * @param mem Available memory.
     * @param disk Available disk space.
     */
    void addQueueEnd(uint32_t mem, uint32_t disk, uint32_t power, Time end);

    void setMinQueueLength(Time q) {
        minQueue = q;
    }

    Time getMinQueueLength() const {
        return minQueue;
    }

    /**
     * Aggregates an TimeConstraintInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const QueueBalancingInfo & o);

    void reduce() {
        summary.clusterize(numClusters);
    }

    // This is documented in AvailabilityInformation.h
    bool operator==(const QueueBalancingInfo & r) const {
        return minQueue == r.minQueue && summary == r.summary;
    }

    /**
     * Returns the longest queue after assigning a certain number of tasks with certain requirements
     * @param clusters The list where the actual clusters will be put.
     * @param numTasks The number of tasks to be allocated.
     * @param req The TaskDescription for the task.
     * @return The end time of the longest queue.
     */
    Time getAvailability(std::list<MDPTCluster *> & clusters, unsigned int numTasks, const TaskDescription & req);

    unsigned int getAvailability(std::list<MDPTCluster *> & clusters, const TaskDescription & req);

    void updateAvailability(const TaskDescription & req);

    // This is documented in BasicMsg
    void output(std::ostream & os) const {
        os << minQueue << ',' << summary;
    }

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("QueueBalancingInfo");
    }
};

#endif /*QUEUEBALANCINGINFO_H_*/


