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

#ifndef BASICAVAILABILITYINFO_H_
#define BASICAVAILABILITYINFO_H_

#include <cmath>
#include "AvailabilityInformation.hpp"
#include "ClusteringVector.hpp"
#include "TaskDescription.hpp"


/**
 * \brief Basic information about node capabilities
 */
class BasicAvailabilityInfo: public AvailabilityInformation {
    /// Set the basic elements for a Serializable descendant
    SRLZ_API SRLZ_METHOD() {
        ar & SERIALIZE_BASE(AvailabilityInformation) & summary & minM & maxM & minD & maxD;
        if (IS_LOADING())
            for (unsigned int i = 0; i < summary.getSize(); i++)
                summary[i].setReference(this);
    }

public:

    class MDCluster {
        SRLZ_API SRLZ_METHOD() {
            ar & value & minM & accumMsq & accumMln & minD & accumDsq & accumDln;
        }

        BasicAvailabilityInfo * reference;

    public:
        uint32_t value;
        uint32_t minM, minD;
        uint64_t accumMsq, accumDsq, accumMln, accumDln;

        MDCluster() : reference(NULL), value(0) {}
        MDCluster(BasicAvailabilityInfo * r, uint32_t m, uint32_t d) : reference(r), value(1), minM(m), minD(d), accumMsq(0), accumDsq(0), accumMln(0), accumDln(0) {}

        void setReference(BasicAvailabilityInfo * r) {
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
                    if (floor((minM - reference->minM) * numIntervals / memRange) != floor((r.minM - reference->minM) * numIntervals / memRange))
                        loss += 100.0;
                    result += loss;
                }
                if (double diskRange = reference->maxD - reference->minD) {
                    double loss = sum.accumDsq / (sum.value * diskRange * diskRange);
                    if (floor((minD - reference->minD) * numIntervals / diskRange) != floor((r.minD - reference->minD) * numIntervals / diskRange))
                        loss += 100.0;
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
            if (newMinM > r.minM) newMinM = r.minM;
            if (newMinD > r.minD) newMinD = r.minD;
            uint64_t dm = minM - newMinM, rdm = r.minM - newMinM;
            accumMsq += value * dm * dm + 2 * dm * accumMln
                        + r.accumMsq + r.value * rdm * rdm + 2 * rdm * r.accumMln;
            accumMln += value * dm + r.accumMln + r.value * rdm;
            uint64_t dd = minD - newMinD, rdd = r.minD - newMinD;
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
    };

private:
    static unsigned int numClusters;
    static unsigned int numIntervals;
    ClusteringVector<MDCluster> summary;
    uint32_t minM, maxM;
    uint32_t minD, maxD;

public:
    static void setNumClusters(unsigned int c) {
        numClusters = c;
        numIntervals = (unsigned int)floor(sqrt(c));
    }

    BasicAvailabilityInfo() {
        reset();
    }
    BasicAvailabilityInfo(const BasicAvailabilityInfo & copy) : AvailabilityInformation(copy), summary(copy.summary),
            minM(copy.minM), maxM(copy.maxM), minD(copy.minD), maxD(copy.maxD) {
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].setReference(this);
    }

    void reset() {
        summary.clear();
        minM = minD = maxM = maxD = 0;
    }

    // This is described in BasicMsg
    virtual BasicAvailabilityInfo * clone() const {
        return new BasicAvailabilityInfo(*this);
    }

    /**
     * Aggregates an ExecutionNodeInfo to this object.
     * @param o The other instance to be aggregated.
     */
    void join(const BasicAvailabilityInfo & r) {
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
        summary.clusterize(numClusters);
    }

    // This is documented in AvailabilityInformation.h
    bool operator==(const BasicAvailabilityInfo & r) const {
        return r.summary == summary;
    }

    void getAvailability(std::list<MDCluster *> & clusters, const TaskDescription & req) {
        for (unsigned int i = 0; i < summary.getSize(); i++) {
            if (summary[i].fulfills(req)) clusters.push_back(&summary[i]);
        }
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

    // This is documented in BasicMsg
    std::string getName() const {
        return std::string("BasicAvailabilityInfo");
    }
};

#endif /* BASICAVAILABILITYINFO_H_ */