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

#ifndef FOURSPAVAILINFO_HPP_
#define FOURSPAVAILINFO_HPP_

#include "AvailabilityInformation.hpp"
#include "ClusteringVector.hpp"
#include "TaskDescription.hpp"
#include "ScalarParameter.hpp"


class FourSPAvailInfo: public AvailabilityInformation {
public:

    class Cluster {
    public:
        Cluster() : reference(NULL), value(0) {}
        Cluster(FourSPAvailInfo * r, uint32_t m, uint32_t d, uint32_t s, Duration t)
            : reference(r), value(1), minM(m), minD(d), minS(s), maxQ(t.seconds()) {}

        void setReference(FourSPAvailInfo * r) {
            reference = r;
        }

        bool operator==(const Cluster & r) const {
            return minM == r.minM && minD == r.minD && minS == r.minS && maxQ == r.maxQ && value == r.value;
        }

        double distance(const Cluster & r, Cluster & sum) const {
            sum = *this;
            sum.aggregate(r);
            if (reference) {
                return sum.minM.norm(reference->memoryRange, sum.value)
                        + sum.minD.norm(reference->diskRange, sum.value)
                        + sum.minS.norm(reference->powerRange, sum.value)
                        + sum.maxQ.norm(reference->queueRange, sum.value);
            } else {
                return 0.0;
            }
        }

        bool far(const Cluster & r) const {
            return minM.far(r.minM, reference->memoryRange, numIntervals) ||
                    minD.far(r.minD, reference->diskRange, numIntervals) ||
                    minS.far(r.minS, reference->powerRange, numIntervals) ||
                    maxQ.far(r.maxQ, reference->queueRange, numIntervals);
        }

        void aggregate(const Cluster & r) {
            // Update minimums/maximums and sum up counts
            minM.aggregate(value, r.minM, r.value);
            minD.aggregate(value, r.minD, r.value);
            minS.aggregate(value, r.minS, r.value);
            maxQ.aggregate(value, r.maxQ, r.value);
            value += r.value;
        }

        int64_t getTotalMemory() const {
            return minM.getValue() * value;
        }

        int64_t getTotalDisk() const {
            return minD.getValue() * value;
        }

        int64_t getTotalSpeed() const {
            return minS.getValue() * value;
        }

        Duration getTotalQueue() const {
            return Duration(maxQ.getValue() * value);
        }

        bool fulfills(const TaskDescription & req) const {
            return minM.getValue() >= req.getMaxMemory() && minD.getValue() >= req.getMaxDisk();
        }

        friend std::ostream & operator<<(std::ostream & os, const Cluster & o) {
            os << 'M' << o.minM << ',';
            os << 'D' << o.minD << ',';
            return os << o.value;
        }

        MSGPACK_DEFINE(value, minM, minD, minS, maxQ);

    private:
        friend class ClusteringVector<Cluster>;

        FourSPAvailInfo * reference;

        uint32_t value;
        MinParameter<int32_t> minM, minD, minS;
        MaxParameter<double> maxQ;
    };

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(sqrt(c)));
    }

    MESSAGE_SUBCLASS(FourSPAvailInfo);

    FourSPAvailInfo() {
        reset();
    }
    FourSPAvailInfo(const FourSPAvailInfo & copy) : AvailabilityInformation(copy),
            summary(copy.summary), memoryRange(copy.memoryRange), diskRange(copy.diskRange),
            powerRange(copy.powerRange), queueRange(copy.queueRange) {}

    void reset() {
        summary.clear();
        memoryRange.setLimits(0);
        diskRange.setLimits(0);
        powerRange.setLimits(0);
        queueRange.setLimits(0.0);
    }

    void setQueueEnd(uint32_t mem, uint32_t disk, uint32_t power, Duration end) {
        summary.clear();
        memoryRange.setLimits(mem);
        diskRange.setLimits(disk);
        powerRange.setLimits(power);
        queueRange.setLimits(end.seconds());
        summary.pushBack(Cluster(this, mem, disk, power, end));
    }

    void join(const FourSPAvailInfo & r) {
        if (!r.summary.isEmpty()) {
            if (summary.isEmpty()) {
                // operator= forbidden
                memoryRange = r.memoryRange;
                diskRange = r.diskRange;
                powerRange = r.powerRange;
                queueRange = r.queueRange;
            } else {
                memoryRange.extend(r.memoryRange);
                diskRange.extend(r.diskRange);
                powerRange.extend(r.powerRange);
                queueRange.extend(r.queueRange);
            }
            summary.add(r.summary);
        }
    }

    void reduce() {
        for (unsigned int i = 0; i < summary.getSize(); i++)
            summary[i].setReference(this);
        summary.clusterize(numClusters);
    }

    bool operator==(const FourSPAvailInfo & r) const {
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

    void output(std::ostream& os) const {
        os << summary;
    }

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, memoryRange, diskRange, powerRange, queueRange);

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    ClusteringVector<Cluster> summary;
    Interval<int32_t> memoryRange;
    Interval<int32_t> diskRange;
    Interval<int32_t> powerRange;
    Interval<double> queueRange;
};


#endif /* FOURSPAVAILINFO_HPP_ */
