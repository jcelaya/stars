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
public:
    class MDPTCluster {
    public:
        MSGPACK_DEFINE(value, minM, minD, minP, accumM, accumD, accumP, maxT, accumT);

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
    private:
        QueueBalancingInfo * reference;
    };

    enum {
        MINIMUM = 0,
        MEAN_QUEUE,
        MEAN_FULL,
    };

    MESSAGE_SUBCLASS(QueueBalancingInfo);

    QueueBalancingInfo() {
        reset();
    }
    QueueBalancingInfo(const QueueBalancingInfo & copy) : AvailabilityInformation(copy), minQueue(copy.minQueue), maxQueue(copy.maxQueue), summary(copy.summary), minM(copy.minM),
            maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD), minP(copy.minP), maxP(copy.maxP), minT(copy.minT), maxT(copy.maxT) {
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].setReference(this);
    }

    static void setMethod(int method) {
        aggrMethod = method;
    }

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(sqrt(c)));
    }

    void reset() {
        minQueue = Time::getCurrentTime();
        summary.clear();
        minM = maxM = minD = maxD = minP = maxP = 0;
        minT = maxT = maxQueue = minQueue;
    }

    /**
     * Set first cluster of the list
     * @param mem Available memory.
     * @param disk Available disk space.
     */
    void setQueueEnd(uint32_t mem, uint32_t disk, uint32_t power, Time end);

    void setMinQueueLength(Time q) {
        minQueue = q;
    }

    Time getMinQueueLength() const {
        return minQueue;
    }

    void setMaxQueueLength(Time q) {
        maxQueue = q;
    }

    Time getMaxQueueLength() const {
        return maxQueue;
    }

    double getMinPower() const { return minP; }

    /**
     * Aggregates an TimeConstraintInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const QueueBalancingInfo & o);

    virtual void reduce() {
        for (unsigned int i = 0; i < summary.getSize(); i++)
            summary[i].setReference(this);
        summary.clusterize(numClusters);
    }

    // This is documented in AvailabilityInformation.h
    bool operator==(const QueueBalancingInfo & r) const {
        return minQueue == r.minQueue && maxQueue == r.maxQueue && summary == r.summary;
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

    void updateMaxT(Time m) { if (maxT < m) maxT = m; }

    // This is documented in BasicMsg
    void output(std::ostream & os) const {
        os << minQueue << ',' << summary;
    }

    MSGPACK_DEFINE((AvailabilityInformation &)*this, minQueue/*, maxQueue*/, summary, minM, maxM, minD, maxD, minP, maxP, minT, maxT);
private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    static int aggrMethod;
    Time minQueue, maxQueue;
    ClusteringVector<MDPTCluster> summary;   ///< List clusters representing queues
    uint32_t minM, maxM, minD, maxD, minP, maxP;
    Time minT, maxT;
};

#endif /*QUEUEBALANCINGINFO_H_*/


