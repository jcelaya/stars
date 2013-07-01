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
    /// Data structures for the aggregation algorithm
    struct DistanceList {
        struct DistanceTo {
            double d;
            unsigned long int v;
            T * to;
            T * sum;
            DistanceTo() {}
            DistanceTo(double _d, T * _t, T * _s) : d(_d), v(_t->value), to(_t), sum(_s) {}

            void invalidate() {
                to->value = 0;
            }

            bool valid() const {
                return to->value > 0;
            }
        };

        DistanceList() : src(NULL), dsts_size(0), dirty(false) {
            dsts.reset(new DistanceTo[K]);
            dst = dsts.get();
        }

        void reset() {
            dsts_size = 0;
            dirty = false;
            dst = dsts.get();
            src = NULL;
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

        bool empty() const {
            return dst == dsts.get() + dsts_size;
        }

        void advance() {
            while (dst < dsts.get() + dsts_size && dst->to->value == 0)
                dst++;
        }

        bool checkConsistency() {
            return dst->v == dst->to->value;
        }

        void updateDistance() {
            dst->d = src->distance(*dst->to, *dst->sum);
            dst->v = dst->to->value;
        }

        T * src;
        boost::scoped_array<DistanceTo> dsts;
        unsigned int dsts_size;
        DistanceTo * dst;
        bool dirty;
    };

    static bool compDL(const DistanceList * l, const DistanceList * r) {
        return l->dsts_size == 0 || (r->dsts_size > 0 && l->dst->d > r->dst->d);
    }

    class VectorOfDistances {
    public:
        VectorOfDistances(ClusteringList & s) : source(s), useFarClusters(false) {}

        size_t populate() {
            size_t size = source.size();
            if (!heap.get()) {
                heap.reset(new DistanceList *[size]);
                sumPool.reset(new T[size * K + 1]);
                lists.reset(new DistanceList[size]);
            } else {
                for (DistanceList * dlsi = lists.get(); dlsi < lists.get() + size; ++dlsi) {
                    dlsi->reset();
                }
            }
            T * top = sumPool.get(), * free = top;
            auto i = source.begin();
            DistanceList ** distancesi = heap.get();
            for (DistanceList * dlsi = lists.get(); dlsi < lists.get() + size; ++dlsi) {
                dlsi->src = &(*i++);
                // Calculate distance with K nearest
                for (auto j = i; j != source.end(); ++j) {
                    if (useFarClusters || !dlsi->src->far(*j)) {
                        dlsi->add(dlsi->src->distance(*j, *free), &(*j), free, top);
                    }
                }
                if (!dlsi->empty()) {
                    *(distancesi++) = dlsi;
                }
            }
            checkNumAdditions();
            size_t result = distancesi - heap.get();
            std::make_heap(heap.get(), distancesi, compDL);
            return result;
        }

        DistanceList ** getHeapPointer() const {
            return heap.get();
        }

    private:
        ClusteringList & source;
        bool useFarClusters;
        boost::scoped_array<DistanceList> lists;
        boost::scoped_array<DistanceList *> heap;
        boost::scoped_array<T> sumPool;

        void checkNumAdditions() {
            if (!useFarClusters) {
                unsigned int numAdditions = 0;
                for (DistanceList * dlsi = lists.get(); dlsi < lists.get() + source.size(); dlsi++) {
                    numAdditions += dlsi->dsts_size;
                }
                unsigned int numMissing = (K + 1) * K / 2;
                if (numAdditions < source.size()*K - numMissing)
                    useFarClusters = true;
            }
        }

    };

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
        VectorOfDistances vod(*this);
        while (this->size() > limit) {
            size_t heapSize = vod.populate();
            DistanceList ** distanceHeap = vod.getHeapPointer();

            unsigned int numClusters = this->size() - limit, clustersJoined = 0;
            while (heapSize > 0 && clustersJoined < numClusters &&
                    distanceHeap[0]->dst->d != std::numeric_limits<double>::infinity()) {
                std::pop_heap(distanceHeap, distanceHeap + heapSize, compDL);
                DistanceList & best = *distanceHeap[heapSize - 1];

                // Ignore it if it is an empty cluster (it has been invalidated before)
                if (best.src->value == 0) {
                    heapSize--;
                } else {
                    if (best.dst->valid()) {
                        if (!best.checkConsistency()) {
                            best.updateDistance();
                            std::push_heap(distanceHeap, distanceHeap + heapSize, compDL);
                            continue;
                        }

                        best.dirty = true;
                        // Join clusters into the first one
                        *best.src = std::move(*best.dst->sum);
                        best.dst->invalidate();
                        clustersJoined++;
                    }
                    best.advance();
                    if (!best.empty()) {
                        if (best.dirty || !best.checkConsistency()) {
                            best.updateDistance();
                        }
                        std::push_heap(distanceHeap, distanceHeap + heapSize, compDL);
                    } else {
                        heapSize--;
                    }
                }
            }
            if (clustersJoined == 0) break;
            purge();
        }
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
};

template<class T> unsigned int ClusteringList<T>::K = 10;

} // namespace stars

#endif /* CLUSTERINGLIST_HPP_ */
