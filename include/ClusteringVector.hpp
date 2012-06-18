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

#ifndef CLUSTERINGVECTOR_H_
#define CLUSTERINGVECTOR_H_

#include <utility>
#include <algorithm>
#include <fstream>
#include <limits>
#include <boost/scoped_array.hpp>
#include <msgpack.hpp>
#include "Logger.hpp"


/**
 * \brief Defines a vector of sample clusters
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
template<class T> class ClusteringVector {
public:
    static void setDistVectorSize(unsigned int k) {
        K = k;
    }

    ClusteringVector() : size(0) {}

    /// Construct an uninitialized vector
    ClusteringVector(size_t newSize) : size(newSize) {
        if (size > 0) {
            buffer.reset(new T[size]);
        }
    }

    ClusteringVector(const ClusteringVector<T> & copy) : size(copy.size) {
        if (size > 0) {
            buffer.reset(new T[size]);
            std::copy(copy.buffer.get(), copy.buffer.get() + size, buffer.get());
        }
    }

    ClusteringVector & operator=(const ClusteringVector<T> & copy) {
        size = copy.size;
        if (size > 0) {
            buffer.reset(new T[size]);
            std::copy(copy.buffer.get(), copy.buffer.get() + size, buffer.get());
        } else buffer.reset();
        return *this;
    }

    size_t getSize() const {
        return size;
    }

    const T & operator[](size_t i) const {
        return buffer[i];
    }
    T & operator[](size_t i) {
        return buffer[i];
    }

    void clear() {
        buffer.reset();
        size = 0;
    }

    bool isEmpty() const {
        return size == 0;
    }

    bool operator==(const ClusteringVector<T> & r) const {
        if (size != r.size) return false;
        else
            for (size_t i = 0; i < size; i++)
                if (!(buffer[i] == r.buffer[i])) return false;
        return true;
    }

    void add(const ClusteringVector<T> & r) {
        if (r.size == 0) return;
        T * tmp = new T[size + r.size];
        std::copy(buffer.get(), buffer.get() + size, tmp);
        std::copy(r.buffer.get(), r.buffer.get() + r.size, tmp + size);
        size += r.size;
        if (size > 1000000) {
            LogMsg("Ex.RI.Aggr", WARN) << "Cluster vector size over 1000000 after adding vector, is it correct??";
        }
        buffer.reset(tmp);
    }

    void merge(const ClusteringVector<T> & r) {
        if (r.size == 0) return;
        T * tmp = new T[size + r.size];
        std::merge(buffer.get(), buffer.get() + size, r.buffer.get(), r.buffer.get() + r.size, tmp);
        size += r.size;
        if (size > 1000000) {
            LogMsg("Ex.RI.Aggr", WARN) << "Cluster vector size over 1000000 after adding vector, is it correct??";
        }
        buffer.reset(tmp);
    }

    void pushBack(const T & t) {
        T * tmp = new T[size + 1];
        std::copy(buffer.get(), buffer.get() + size, tmp);
        tmp[size] = t;
        size++;
        if (size > 1000000) {
            LogMsg("Ex.RI.Aggr", WARN) << "Cluster vector size over 1000000 after adding element, is it correct??";
        }
        buffer.reset(tmp);
    }

    void purge() {
        // Remove clusters with value equal to zero
        if (size == 0) return;
        size_t zeroes = 0;
        for (T * i = buffer.get(); i < buffer.get() + size; i++)
            if (i->value == 0)
                zeroes++;
        if (zeroes == 0) return;
        size -= zeroes;
        if (size > 1000000) {
            LogMsg("Ex.RI.Aggr", WARN) << "Cluster vector size over 1000000 after purging, is it correct??";
        }
        T * tmp = new T[size];
        for (T * i = buffer.get(), * j = tmp; j < tmp + size; i++)
            if (i->value != 0)
                *j++ = *i;
        buffer.reset(tmp);
    }

    void clusterize(size_t limit) {
        bool useFar = false;
        // Check if we need to perform clustering
        while (size > limit) {
            LogMsg("Ex.RI.Aggr", DEBUG) << "Clusterizing";
            // Make vector of distances
            size_t distSize = size - 1;
            boost::scoped_array<DistanceList> tmpDls(new DistanceList[distSize]);
            boost::scoped_array<DistanceList *> tmpDistances(new DistanceList *[distSize]);
            boost::scoped_array<T> tmpSums(new T[distSize * K + 1]);
            T * top = tmpSums.get(), * free = tmpSums.get();
            DistanceList * dls = tmpDls.get();
            DistanceList ** distances = tmpDistances.get();
            {
                unsigned int additions = 0;
                T * i = buffer.get();
                T * last = buffer.get() + size;
                for (DistanceList * dlsi = dls, ** distancesi = distances; dlsi < dls + distSize; dlsi++) {
                    *(distancesi++) = dlsi;
                    dlsi->src = i++;
                    // Calculate distance with K nearest
                    for (T * j = i; j < last; j++) {
                        if (useFar || !dlsi->src->far(*j)) {
                            dlsi->add(dlsi->src->distance(*j, *free), j, free, top);
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
            unsigned int numClusters = size - limit;
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
        LogMsg("Ex.RI.Aggr", DEBUG) << "We end up with " << size << " clusters:";
        LogMsg("Ex.RI.Aggr", DEBUG) << *this;
    }

    friend std::ostream & operator<<(std::ostream & os, const ClusteringVector & o) {
        for (unsigned int i = 0; i < o.size; i++)
            os << '(' << o.buffer[i] << ')';
        return os;
    }

    template <typename Packer> void msgpack_pack(Packer& pk) const {
        pk.pack_array(size + 1);
        pk.pack(size);
        for (size_t i = 0; i < size; ++i)
            pk.pack(buffer[i]);
    }
    void msgpack_unpack(msgpack::object o) {
        if(o.type != msgpack::type::ARRAY) { throw msgpack::type_error(); }
        o.via.array.ptr[0].convert(&size);
        if (size) buffer.reset(new T[size]);
        else buffer.reset();
        for (size_t i = 0; i < size; i++)
            o.via.array.ptr[i + 1].convert(&buffer[i]);
    }
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
    
    boost::scoped_array<T> buffer;
    size_t size;
};

template<class T> unsigned int ClusteringVector<T>::K = 10;

#endif /* CLUSTERINGVECTOR_H_ */
