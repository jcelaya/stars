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
#include "ClusteringVector.hpp"
#include "TaskDescription.hpp"


/**
 * \brief Basic information about node capabilities
 */
class IBPAvailabilityInformation: public AvailabilityInformation {
public:

    class MDCluster {
    public:
        MDCluster() : reference(NULL), value(0) {}
        MDCluster(IBPAvailabilityInformation * r, uint32_t m, uint32_t d) : reference(r), value(1), minM(m), minD(d), accumMsq(0), accumDsq(0), accumMln(0), accumDln(0) {}

        void setReference(IBPAvailabilityInformation * r) {
            reference = r;
        }

        bool operator==(const MDCluster & r) const {
            return minM == r.minM && accumMsq == r.accumMsq && accumMln == r.accumMln
                   && minD == r.minD && accumDsq == r.accumDsq && accumDln == r.accumDln
                   && value == r.value;
        }

        double distance(const MDCluster & r, MDCluster & sum) const {
            sum = *this;
            sum.aggregate(r);
            double result = 0.0;
            if (reference) {
                if (double memRange = reference->maxM - reference->minM) {
                    double loss = sum.accumMsq / (sum.value * memRange * memRange);
                    result += loss;
                }
                if (double diskRange = reference->maxD - reference->minD) {
                    double loss = sum.accumDsq / (sum.value * diskRange * diskRange);
                    result += loss;
                }
            }
            return result;
        }

        bool far(const MDCluster & r) const {
            if (unsigned int memRange = reference->maxM - reference->minM) {
                if (((minM - reference->minM) * numIntervals / memRange) != ((r.minM - reference->minM) * numIntervals / memRange))
                    return true;
            }
            if (unsigned int diskRange = reference->maxD - reference->minD) {
                if (((minD - reference->minD) * numIntervals / diskRange) != ((r.minD - reference->minD) * numIntervals / diskRange))
                    return true;
            }
            return false;
        }

        void aggregate(const MDCluster & r) {
            // Update minimums/maximums and sum up values
            uint32_t newMinD = minD, newMinM = minM;
            switch (aggrMethod) {
            case MEAN:
                newMinM = (minM * value + r.minM * r.value) / (value + r.value);
                newMinD = (minD * value + r.minD * r.value) / (value + r.value);
                break;
            default:
                if (newMinM > r.minM) newMinM = r.minM;
                if (newMinD > r.minD) newMinD = r.minD;
                break;
            }
            int64_t dm = minM - newMinM, rdm = r.minM - newMinM;
            accumMsq += value * dm * dm + 2 * dm * accumMln
                        + r.accumMsq + r.value * rdm * rdm + 2 * rdm * r.accumMln;
            accumMln += value * dm + r.accumMln + r.value * rdm;
            int64_t dd = minD - newMinD, rdd = r.minD - newMinD;
            accumDsq += value * dd * dd + 2 * dd * accumDln
                        + r.accumDsq + r.value * rdd * rdd + 2 * rdd * r.accumDln;
            accumDln += value * dd + r.accumDln + r.value * rdd;
            minM = newMinM;
            minD = newMinD;
            value += r.value;
        }

        bool fulfills(const TaskDescription & req) const {
            return minM >= req.getMaxMemory() && minD >= req.getMaxDisk();
        }

        friend std::ostream & operator<<(std::ostream & os, const MDCluster & o) {
            os << 'M' << o.minM << '+' << o.accumMsq << '+' << o.accumMln << ',';
            os << 'D' << o.minD << '+' << o.accumDsq << '+' << o.accumDln << ',';
            return os << o.value;
        }

        MSGPACK_DEFINE(value, minM, minD, accumMsq, accumDsq, accumMln, accumDln);

        IBPAvailabilityInformation * reference;

        uint32_t value;
        uint32_t minM, minD;
        int64_t accumMsq, accumDsq, accumMln, accumDln;
    };

    enum {
        MINIMUM = 0,
        MEAN,
    };

    static void setMethod(int method) {
        aggrMethod = method;
    }

    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(c));
    }

    MESSAGE_SUBCLASS(IBPAvailabilityInformation);

    IBPAvailabilityInformation() {
        reset();
    }
    IBPAvailabilityInformation(const IBPAvailabilityInformation & copy) : AvailabilityInformation(copy), summary(copy.summary),
            minM(copy.minM), maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD) {
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].setReference(this);
    }

    void reset() {
        summary.clear();
        minM = minD = maxM = maxD = 0;
    }

    /**
     * Aggregates an ExecutionNodeInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const IBPAvailabilityInformation & r) {
        if (!r.summary.isEmpty()) {
            if (summary.isEmpty()) {
                // operator= forbidden
                minM = r.minM;
                maxM = r.maxM;
                minD = r.minD;
                maxD = r.maxD;
            } else {
                if (minM > r.minM) minM = r.minM;
                if (maxM < r.maxM) maxM = r.maxM;
                if (minD > r.minD) minD = r.minD;
                if (maxD < r.maxD) maxD = r.maxD;
            }
            summary.add(r.summary);
            unsigned int size = summary.getSize();
            for (unsigned int i = 0; i < size; i++)
                summary[i].setReference(this);
        }
    }

    void reduce() {
        for (unsigned int i = 0; i < summary.getSize(); i++)
            summary[i].setReference(this);
        summary.cluster(numClusters);
    }

    // This is documented in AvailabilityInformation.h
    bool operator==(const IBPAvailabilityInformation & r) const {
        return r.summary == summary;
    }

    void getAvailability(std::list<MDCluster *> & clusters, const TaskDescription & req) {
        for (unsigned int i = 0; i < summary.getSize(); i++) {
            if (summary[i].fulfills(req)) clusters.push_back(&summary[i]);
        }
    }

    void updated() {
        summary.purge();
    }

    void addNode(uint32_t mem, uint32_t disk) {
        if (summary.isEmpty()) {
            minM = mem;
            maxM = mem;
            minD = disk;
            maxD = disk;
        } else {
            if (minM > mem) minM = mem;
            if (maxM < mem) maxM = mem;
            if (minD > disk) minD = disk;
            if (maxD < disk) maxD = disk;
        }
        summary.pushBack(MDCluster(this, mem, disk));
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {
        os << summary;
    }

    MSGPACK_DEFINE((AvailabilityInformation &)*this, summary, minM, maxM, minD, maxD);

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    static int aggrMethod;
    ClusteringVector<MDCluster> summary;
    uint32_t minM, maxM;
    uint32_t minD, maxD;
};

#endif /* IBPAVAILABILITYINFORMATION_H_ */
