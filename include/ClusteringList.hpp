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

#ifndef CLUSTERINGLIST_HPP_
#define CLUSTERINGLIST_HPP_

#include <list>
#include <boost/scoped_array.hpp>
#include "Logger.hpp"


namespace stars {

/**
 * \brief Defines a list of sample clusters
 *
 * A sample cluster is a class/struct with, at least:
 * <ul>
 *     <li>A value field with the number of samples in the cluster</li>
 *     <li>A method distance that returns the distance to another cluster and precalculates the aggregation</li>
 *     <li>A method far that returns a fast distance calculation</li>
 *     <li>A method aggregate that aggregates another cluster into it</li>
 *     <li>A method to output a representation of such object</li>
 *     <li>Serializable interface</li>
 * </ul>
 */
template<class T> class ClusteringList : public std::list<T> {
public:
    static void setDistVectorSize(unsigned int k) {
        K = k;
    }

    void purge() {
        // Remove clusters with value equal to zero
        for (auto i = this->begin(); i != this->end();) {
            if (i->value == 0) {
                i = this->erase(i);
            } else {
                ++i;
            }
        }
    }

    void cluster(size_t limit) {
        bool useFar = false;
        // Check if we need to perform clustering
        while (this->size() > limit) {
            LogMsg("Ex.RI.Aggr", DEBUG) << "Clusterizing";
            // Make vector of distances
            size_t distSize = this->size() - 1;
            boost::scoped_array<DistanceList> tmpDls(new DistanceList[distSize]);
            boost::scoped_array<DistanceList *> tmpDistances(new DistanceList *[distSize]);
            boost::scoped_array<T> tmpSums(new T[distSize * K + 1]);
            T * top = tmpSums.get(), * free = tmpSums.get();
            DistanceList * dls = tmpDls.get();
            DistanceList ** distances = tmpDistances.get();
            {
                unsigned int additions = 0;
                auto i = this->begin();
                for (DistanceList * dlsi = dls, ** distancesi = distances; dlsi < dls + distSize; dlsi++) {
                    *(distancesi++) = dlsi;
                    dlsi->src = &(*i++);
                    // Calculate distance with K nearest
                    for (auto j = i; j != this->end(); ++j) {
                        if (useFar || !dlsi->src->far(*j)) {
                            dlsi->add(dlsi->src->distance(*j, *free), &(*j), free, top);
                            additions++;
                        }
                    }
                }
                if (additions < distSize * K) useFar = true;
            }
            std::make_heap(distances, distances + distSize, compDL);
            // We are done if the shortest distance is infinite
            if (distances[0]->dst->d == std::numeric_limits<double>::infinity()) break;

            // Make rsize - MAX_CLUSTERS new clusters
            unsigned int numClusters = this->size() - limit;
            for (unsigned int count = 0; distSize > 0 && count < numClusters;) {
                // Get best distance
                std::pop_heap(distances, distances + distSize, compDL);
                DistanceList & best = *distances[distSize - 1];

                // We are done if we reach an empty list or an infinite distance
                if (best.dsts_size == 0 || best.dst->d == std::numeric_limits<double>::infinity()) break;

                // Ignore it if it is an empty cluster (it has been invalidated before)
                if (best.src->value == 0) {
                    distSize--;
                    continue;
                }

                // Ignore this pair if the second cluster is invalid
                if (best.dst->to->value > 0) {
                    // Check whether the second cluster has joined another one
                    if (best.dst->v != best.dst->to->value) {
                        // Update the distance and repush this cluster
                        best.dst->d = best.src->distance(*best.dst->to, *best.dst->sum);
                        best.dst->v = best.dst->to->value;
                        std::push_heap(distances, distances + distSize, compDL);
                        continue;
                    }

                    best.dirty = true;
                    LogMsg("Ex.RI.Aggr", DEBUG) << "Joining (" << *best.src << ") and (" << *best.dst->to << ") with distance " << best.dst->d;
                    // Join clusters into the first one
                    if (best.dst->sum->value > 0) {
                        *best.src = *best.dst->sum;
                    } else
                        best.src->aggregate(*best.dst->to);
                    LogMsg("Ex.RI.Aggr", DEBUG) << "The result is (" << *best.src << ')';
                    // Invalidate the second one
                    best.dst->to->value = 0;
                    count++;
                }
                // Jump over invalid clusters
                while (best.dst < best.dsts.get() + best.dsts_size && best.dst->to->value == 0)
                    best.dst++;
                // If there is still information to aggregate this cluster, repush it
                if (best.dst < best.dsts.get() + best.dsts_size) {
                    if (best.dirty || best.dst->v != best.dst->to->value) {
                        // Update the distance
                        best.dst->d = best.src->distance(*best.dst->to, *best.dst->sum);
                        best.dst->v = best.dst->to->value;
                    }
                    std::push_heap(distances, distances + distSize, compDL);
                } else {
                    distSize--;
                }
            }

            // Remove invalid clusters
            purge();
        }
        LogMsg("Ex.RI.Aggr", DEBUG) << "We end up with " << this->size() << " clusters:";
        LogMsg("Ex.RI.Aggr", DEBUG) << *this;
    }

    friend std::ostream & operator<<(std::ostream & os, const ClusteringList & o) {
        for (auto & i : o)
            os << '(' << i << ')';
        return os;
    }

    MSGPACK_DEFINE((std::list<T> &)*this);

private:
    /// Maximum size of the vector of samples
    static unsigned int K;

    /// Data structures for the aggregation algorithm
    struct DistanceList {
        struct DistanceTo {
            double d;
            unsigned long int v;
            T * to;
            T * sum;
            DistanceTo() {}
            DistanceTo(double _d, T * _t, T * _s) : d(_d), v(_t->value), to(_t), sum(_s) {}
        };

        T * src;
        boost::scoped_array<DistanceTo> dsts;
        unsigned int dsts_size;
        DistanceTo * dst;
        bool dirty;
        DistanceList() : dsts_size(0), dirty(false) {
            dsts.reset(new DistanceTo[K]);
            dst = dsts.get();
        }
        bool add(double d, T * _dst, T * & sum, T * & top) {
            if (dsts_size < K || d < dsts[dsts_size - 1].d) {
                // insert it
                if (top == sum) top++;
                unsigned int i;
                T * free;
                if (dsts_size < K) {
                    i = dsts_size++;
                    free = top;
                } else {
                    i = K - 1;
                    free = dsts[i].sum;
                }
                for (; i > 0 && dsts[i - 1].d > d; i--)
                    dsts[i] = dsts[i - 1];
                dsts[i] = DistanceTo(d, _dst, sum);
                sum = free;
                return true;
            }
            return false;
        }
    };

    static bool compDL(const DistanceList * l, const DistanceList * r) {
        return l->dsts_size == 0 || (r->dsts_size > 0 && l->dst->d > r->dst->d);
    }
};

template<class T> unsigned int ClusteringList<T>::K = 10;

} // namespace stars

#endif /* CLUSTERINGLIST_HPP_ */
