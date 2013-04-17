/*  STaRS, Scalable Task Routing approach to distributed Scheduling
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

#ifndef TWOSPAVAILINFO_HPP_
#define TWOSPAVAILINFO_HPP_

#include "AvailabilityInformation.hpp"
#include "ClusteringVector.hpp"
#include "TaskDescription.hpp"
#include "ScalarParameter.hpp"


class TwoSPAvailInfo: public AvailabilityInformation {
public:

    class Cluster {
    public:
        Cluster() : reference(NULL), value(0) {}
        Cluster(TwoSPAvailInfo * r, uint32_t m, uint32_t d) : reference(r), value(1), minM(m), minD(d) {}

        void setReference(TwoSPAvailInfo * r) {
            reference = r;
        }

        bool operator==(const Cluster & r) const {
            return minM == r.minM && minD == r.minD && value == r.value;
        }

        double distance(const Cluster & r, Cluster & sum) const {
            sum = *this;
            sum.aggregate(r);
            if (reference) {
                return sum.minM.norm(reference->memoryRange, sum.value)
                        + sum.minD.norm(reference->diskRange, sum.value);
            } else {
                return 0.0;
            }
        }

        bool far(const Cluster & r) const {
            return minM.far(r.minM, reference->memoryRange, numIntervals) ||
                    minD.far(r.minD, reference->diskRange, numIntervals);
        }

        void aggregate(const Cluster & r) {
            // Update minimums/maximums and sum up counts
            minM.aggregate(value, r.minM, r.value);
            minD.aggregate(value, r.minD, r.value);
            value += r.value;
        }

        int64_t getTotalMemory() const {
            return minM.getValue() * value;
        }

        int64_t getTotalDisk() const {
            return minD.getValue() * value;
        }

        bool fulfills(const TaskDescription & req) const {
            return minM.getValue() >= req.getMaxMemory() && minD.getValue() >= req.getMaxDisk();
        }

        friend std::ostream & operator<<(std::ostream & os, const Cluster & o) {
            os << 'M' << o.minM << ',';
            os << 'D' << o.minD << ',';
            return os << o.value;
        }

        MSGPACK_DEFINE(value, minM, minD);

    private:
        friend class ClusteringVector<Cluster>;

        TwoSPAvailInfo * reference;

        uint32_t value;
        MinParameter<int32_t> minM, minD;
    };

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(c));
    }

    MESSAGE_SUBCLASS(TwoSPAvailInfo);

    TwoSPAvailInfo() {
        reset();
    }
    TwoSPAvailInfo(const TwoSPAvailInfo & copy) : AvailabilityInformation(copy),
            summary(copy.summary), memoryRange(copy.memoryRange), diskRange(copy.diskRange) {}

    void reset() {
        summary.clear();
        memoryRange.setLimits(0);
        diskRange.setLimits(0);
    }

    void join(const TwoSPAvailInfo & r) {
        if (!r.summary.isEmpty()) {
            if (summary.isEmpty()) {
                // operator= forbidden
                memoryRange = r.memoryRange;
                diskRange = r.diskRange;
            } else {
                memoryRange.extend(r.memoryRange);
                diskRange.extend(r.diskRange);
            }
            summary.add(r.summary);
        }
    }

    void reduce() {
        for (unsigned int i = 0; i < summary.getSize(); i++)
            summary[i].setReference(this);
        summary.clusterize(numClusters);
    }

    bool operator==(const TwoSPAvailInfo & r) const {
        return r.summary == summary;
    }

    void getAvailability(std::list<Cluster *> & clusters, const TaskDescription & req) {
        for (unsigned int i = 0; i < summary.getSize(); i++) {
            if (summary[i].fulfills(req)) {
                clusters.push_back(&summary[i]);
            }
        }
    }

    void updated() {
        summary.purge();
    }

    void addNode(uint32_t mem, uint32_t disk) {
        if (summary.isEmpty()) {
            memoryRange.setLimits(mem);
            diskRange.setLimits(disk);
        } else {
            memoryRange.extend(mem);
            diskRange.extend(disk);
        }
        summary.pushBack(Cluster(this, mem, disk));
    }

    void output(std::ostream& os) const {
        os << summary;
    }

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, memoryRange, diskRange);

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    ClusteringVector<Cluster> summary;
    Interval<int32_t> memoryRange;
    Interval<int32_t> diskRange;
};


#endif /* TWOSPAVAILINFO_HPP_ */
