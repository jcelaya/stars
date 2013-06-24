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

#ifndef MMPAVAILABILITYINFORMATION_H_
#define MMPAVAILABILITYINFORMATION_H_

#include <cmath>
#include "Time.hpp"
#include "AvailabilityInformation.hpp"
#include "ClusteringList.hpp"
#include "TaskDescription.hpp"
#include "ScalarParameter.hpp"


template <> inline int64_t Interval<Time, int64_t>::difference(Time M, Time m) {
    return int64_t((M - m).seconds());
}


/**
 * \brief Availability information class with queue length information.
 *
 * This class describes the properties of a set of execution nodes, so that this information
 * may be aggregated and used in the search algorithm.
 */
class MMPAvailabilityInformation : public AvailabilityInformation {
public:
    class MDPTCluster {
    public:
        MSGPACK_DEFINE(value, minM, minD, minP, maxT);

        MDPTCluster() : reference(NULL), value(0) {}
        MDPTCluster(int32_t m, int32_t d, int32_t p, Time t)
                : reference(NULL), value(1), minM(m), minD(d), minP(p), maxT(t) {}

        void setReference(MMPAvailabilityInformation * r) {
            reference = r;
        }

        // Order by maxT value
        bool operator<(const MDPTCluster & r) const {
            return maxT.getValue() < r.maxT.getValue();
        }

        bool operator==(const MDPTCluster & r) const {
            return minM == r.minM && minD == r.minD && minP == r.minP && maxT == r.maxT && value == r.value;
        }

        double distance(const MDPTCluster & r, MDPTCluster & sum) const {
            sum = *this;
            sum.aggregate(r);
            return sum.minM.norm(reference->memoryRange, sum.value)
                    + sum.minD.norm(reference->diskRange, sum.value)
                    + sum.minP.norm(reference->powerRange, sum.value)
                    + sum.maxT.norm(reference->queueRange, sum.value);
        }

        bool far(const MDPTCluster & r) const {
            return minM.far(r.minM, reference->memoryRange, numIntervals) ||
                    minD.far(r.minD, reference->diskRange, numIntervals) ||
                    minP.far(r.minP, reference->powerRange, numIntervals) ||
                    maxT.far(r.maxT, reference->queueRange, numIntervals);
        }

        void aggregate(const MDPTCluster & r) {
            // Update minimums/maximums and sum up counts
            minM.aggregate(value, r.minM, r.value);
            minD.aggregate(value, r.minD, r.value);
            minP.aggregate(value, r.minP, r.value);
            maxT.aggregate(value, r.maxT, r.value);
            value += r.value;
        }

        bool fulfills(const TaskDescription & req) const {
            return minM.getValue() >= req.getMaxMemory() && minD.getValue() >= req.getMaxDisk();
        }

        uint32_t getValue() const {
            return value;
        }

        int32_t getMinimumPower() const {
            return minP.getValue();
        }

        Time getMaximumQueue() const {
            return maxT.getValue();
        }


        int32_t getLostMemory(const TaskDescription & req) const {
            return minM.getValue() - req.getMaxMemory();
        }

        int32_t getLostDisk(const TaskDescription & req) const {
            return minD.getValue() - req.getMaxDisk();
        }

        int64_t getTotalMemory() const {
            return minM.getValue() * value;
        }

        int64_t getTotalDisk() const {
            return minD.getValue() * value;
        }

        int64_t getTotalSpeed() const {
            return minP.getValue() * value;
        }

        Duration getTotalQueue(Time reference) const {
            return (maxT.getValue() - reference) * value;
        }

        static std::string getName() {
            return std::string("MDPTCluster");
        }

        void updateMaximumQueue(Time m) {
            maxT = MaxParameter<Time, int64_t>(m);
        }

        friend std::ostream & operator<<(std::ostream & os, const MDPTCluster & o) {
            return os << 'M' << o.minM << ',' << 'D' << o.minD << ',' << 'P' << o.minP << ',' << 'T' << o.maxT << ',' << o.value;
        }
    private:
        friend class stars::ClusteringList<MDPTCluster>;
        friend class MMPAvailabilityInformation;

        MMPAvailabilityInformation * reference;
        uint32_t value;
        MinParameter<int32_t, int64_t> minM, minD, minP;
        MaxParameter<Time, int64_t> maxT;
    };

    MESSAGE_SUBCLASS(MMPAvailabilityInformation);

    MMPAvailabilityInformation() {
        reset();
    }

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(sqrt(c)));
    }

    void reset() {
        memoryRange.setLimits(0);
        diskRange.setLimits(0);
        powerRange.setLimits(0);
        maxQueue = Time::getCurrentTime();
        queueRange.setLimits(maxQueue);
        summary.clear();
    }

    /**
     * Set first cluster of the list
     * @param mem Available memory.
     * @param disk Available disk space.
     */
    void setQueueEnd(int32_t mem, int32_t disk, int32_t power, Time end);

    void setMaxQueueLength(Time q) {
        maxQueue = q;
    }

    Time getMaxQueueLength() const {
        return maxQueue;
    }

    //double getMinPower() const { return powerRange.getMin(); }

    /**
     * Aggregates an TimeConstraintInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const MMPAvailabilityInformation & o);

    virtual void reduce() {
        for (auto & i : summary)
            i.setReference(this);
        summary.cluster(numClusters);
    }

    // This is documented in AvailabilityInformation.h
    bool operator==(const MMPAvailabilityInformation & r) const {
        return maxQueue == r.maxQueue && summary == r.summary;
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

    void updateMaxT(Time m) { queueRange.extend(m); }

    // This is documented in BasicMsg
    void output(std::ostream & os) const {
        os << maxQueue << ',' << summary;
    }

    MSGPACK_DEFINE((AvailabilityInformation &)*this, maxQueue, summary, memoryRange, diskRange, powerRange, queueRange);
private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    Time maxQueue;
    stars::ClusteringList<MDPTCluster> summary;   ///< List clusters representing queues
    Interval<int32_t, int64_t> memoryRange;
    Interval<int32_t, int64_t> diskRange;
    Interval<int32_t, int64_t> powerRange;
    Interval<Time, int64_t> queueRange;
};

#endif /* MMPAVAILABILITYINFORMATION_H_ */


