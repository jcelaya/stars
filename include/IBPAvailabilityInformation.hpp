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

#ifndef IBPAVAILABILITYINFORMATION_H_
#define IBPAVAILABILITYINFORMATION_H_

#include <cmath>
#include "AvailabilityInformation.hpp"
#include "ClusteringList.hpp"
#include "TaskDescription.hpp"
#include "ScalarParameter.hpp"


/**
 * \brief Basic information about node capabilities
 */
class IBPAvailabilityInformation: public AvailabilityInformation {
public:

    class MDCluster {
    public:
        MDCluster() : reference(NULL), value(0) {}
        MDCluster(uint32_t m, uint32_t d) : reference(NULL), value(1), minM(m), minD(d) {}

        void setReference(IBPAvailabilityInformation * r) {
            reference = r;
        }

        bool operator==(const MDCluster & r) const {
            return minM == r.minM && minD == r.minD && value == r.value;
        }

        double distance(const MDCluster & r, MDCluster & sum) const {
            sum = *this;
            sum.aggregate(r);
            return sum.minM.norm(reference->memoryRange, sum.value)
                    + sum.minD.norm(reference->diskRange, sum.value);
        }

        bool far(const MDCluster & r) const {
            return minM.far(r.minM, reference->memoryRange, numIntervals) ||
                    minD.far(r.minD, reference->diskRange, numIntervals);
        }

        void aggregate(const MDCluster & r) {
            // Update minimums/maximums and sum up values
            minM.aggregate(value, r.minM, r.value);
            minD.aggregate(value, r.minD, r.value);
            value += r.value;
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

        int32_t getRemainingMemory(int32_t m) const {
            return minM.getValue() - m;
        }

        int32_t getRemainingDisk(int32_t d) const {
            return minD.getValue() - d;
        }

        bool fulfills(const TaskDescription & req) const {
            return minM.getValue() >= req.getMaxMemory() && minD.getValue() >= req.getMaxDisk();
        }

        uint32_t takeUpToNodes(uint32_t nodes) {
            if (nodes > value) {
                nodes -= value;
                value = 0;
            } else {
                value -= nodes;
                nodes = 0;
            }
            return nodes;
        }

        friend std::ostream & operator<<(std::ostream & os, const MDCluster & o) {
            return os << 'M' << o.minM << ',' << 'D' << o.minD << ',' << o.value;
        }

        MSGPACK_DEFINE(value, minM, minD);

    private:
        friend class stars::ClusteringList<MDCluster>;

        IBPAvailabilityInformation * reference;

        uint32_t value;
        MinParameter<int32_t, int64_t> minM, minD;
    };

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(c));
    }

    MESSAGE_SUBCLASS(IBPAvailabilityInformation);

    IBPAvailabilityInformation() {
        reset();
    }

    void reset() {
        summary.clear();
        memoryRange.setLimits(0);
        diskRange.setLimits(0);
    }

    /**
     * Aggregates an ExecutionNodeInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const IBPAvailabilityInformation & r) {
        if (!r.summary.empty()) {
            if (summary.empty()) {
                // operator= forbidden
                memoryRange = r.memoryRange;
                diskRange = r.diskRange;
            } else {
                memoryRange.extend(r.memoryRange);
                diskRange.extend(r.diskRange);
            }
            summary.insert(summary.end(), r.summary.begin(), r.summary.end());
        }
    }

    void reduce() {
        for (auto & i : summary) {
            i.setReference(this);
        }
        summary.cluster(numClusters);
    }

    // This is documented in AvailabilityInformation.h
    bool operator==(const IBPAvailabilityInformation & r) const {
        return r.summary == summary;
    }

    void getAvailability(std::list<MDCluster *> & clusters, const TaskDescription & req) {
        for (auto & i : summary) {
            if (i.fulfills(req)) {
                clusters.push_back(&i);
            }
        }
    }

    void updated() {
        summary.purge();
    }

    void addNode(uint32_t mem, uint32_t disk) {
        if (summary.empty()) {
            memoryRange.setLimits(mem);
            diskRange.setLimits(disk);
        } else {
            memoryRange.extend(mem);
            diskRange.extend(disk);
        }
        summary.push_back(MDCluster(mem, disk));
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {
        os << summary;
    }

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, memoryRange, diskRange);

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    stars::ClusteringList<MDCluster> summary;
    Interval<int32_t, int64_t> memoryRange;
    Interval<int32_t, int64_t> diskRange;
};

#endif /* IBPAVAILABILITYINFORMATION_H_ */
