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

#ifndef AGGREGATIONTEST_HPP_
#define AGGREGATIONTEST_HPP_

#include <cmath>
#include <list>
#include <sstream>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "AvailabilityInformation.hpp"
#include "Logger.hpp"
using namespace std;
//using namespace boost;
using namespace boost::posix_time;
//using namespace boost::random;

template<class T> struct Priv;

template<class T> class AggregationTest {
    unsigned int fanout;

    struct Node {
        unsigned int power;
        unsigned int mem;
        unsigned int disk;
        boost::shared_ptr<T> avail;
        size_t size;
    };
    list<Node> nodes;
    unsigned long int totalPower, totalMem, totalDisk;
    boost::random::mt19937 gen;
    boost::random::uniform_int_distribution<> unifPower, unifMemory, unifDisk;
    Priv<T> privateData;

    typename list<Node>::iterator nextNode;
    unsigned long int bytes;
    unsigned long int messages;
    unsigned int maxSize;
    unsigned int minSize;
    // Progress
    unsigned int totalCalls, numCalls;
    ptime lastProgress;
    time_duration aggregationDuration;

    boost::shared_ptr<T> createInfo(const Node & n);

    boost::shared_ptr<T> newNode() {
        if (nextNode != nodes.end()) {
            recordSize(nextNode->size);
            return (nextNode++)->avail;
        } else {
            nodes.push_back(Node());
            Node & n = nodes.back();
            n.power = unifPower(gen); //floor(unifPower(gen) / step_power) * step_power;
            n.mem = unifMemory(gen); //floor(unifMemory(gen) / step_mem) * step_mem;
            n.disk = unifDisk(gen); //floor(unifDisk(gen) / step_disk) * step_disk;
            totalPower += n.power;
            totalMem += n.mem;
            totalDisk += n.disk;
            n.avail = createInfo(n);
            n.avail->reduce();
            n.size = measureSize(n.avail);
            return n.avail;
        }
    }

    size_t measureSize(boost::shared_ptr<AvailabilityInformation> e) {
        ostringstream oss;
        msgpack::packer<std::ostream> pk(&oss);
        e->pack(pk);
        recordSize(oss.tellp());
        return oss.tellp();
    }

    void recordSize(unsigned int size) {
        if (minSize > size) minSize = size;
        if (maxSize < size) maxSize = size;
        bytes += size;
        messages++;
    }

    boost::shared_ptr<T> aggregateLevel(unsigned int level) {
        boost::shared_ptr<T> result;
        if (level == 0) {
            result.reset(newNode()->clone());
            for (unsigned int i = 1; i < fanout; i++) {
                boost::shared_ptr<T> tmp = newNode();
                ptime start = microsec_clock::universal_time();
                result->join(*tmp);
                aggregationDuration += microsec_clock::universal_time() - start;
            }
        } else {
            result = aggregateLevel(level - 1);
            for (unsigned int i = 1; i < fanout; i++) {
                boost::shared_ptr<T> tmp = aggregateLevel(level - 1);
                ptime start = microsec_clock::universal_time();
                result->join(*tmp);
                aggregationDuration += microsec_clock::universal_time() - start;
            }
        }
        ptime start = microsec_clock::universal_time();
        result->reduce();
        measureSize(result);
        aggregationDuration += microsec_clock::universal_time() - start;

        numCalls++;
        unsigned int p = floor(numCalls * 100.0 / totalCalls);
        ptime now = microsec_clock::universal_time();
        if (lastProgress + seconds(1) < now) {
            lastProgress = now;
            LogMsg("Progress", WARN) << p << '%';
        }
        return result;
    }

public:
    static const int min_power = 1000;
    static const int max_power = 3000;
//    static const int step_power = 1;
    static const int min_mem = 256;
    static const int max_mem = 4096;
//    static const int step_mem = 1;
    static const int min_disk = 500;
    static const int max_disk = 5000;
//    static const int step_disk = 1;

    AggregationTest(unsigned int f = 2) : fanout(f), totalPower(0), totalMem(0), totalDisk(0),
            unifPower(min_power, max_power), unifMemory(min_mem, max_mem), unifDisk(min_disk, max_disk) {}

    boost::shared_ptr<T> test(unsigned int numLevels) {
        nextNode = nodes.begin();
        messages = 0;
        maxSize = 0;
        minSize = 1000000000;
        bytes = 0;
        totalCalls = (pow((double)fanout, (int)(numLevels + 1)) - 1) / (fanout - 1);
        numCalls = 0;
        aggregationDuration = seconds(0);
        lastProgress = microsec_clock::universal_time();
        return aggregateLevel(numLevels);
    }

    unsigned int getMinSize() const {
        return minSize;
    }

    unsigned int getMaxSize() const {
        return maxSize;
    }

    unsigned int getMeanSize() const {
        return bytes / messages;
    }

    time_duration getMeanTime() const {
        return aggregationDuration / (messages / 2);
    }

    unsigned long int getTotalPower() const {
        return totalPower;
    }

    unsigned long int getTotalMem() const {
        return totalMem;
    }

    unsigned long int getTotalDisk() const {
        return totalDisk;
    }

    unsigned long int getNumNodes() const {
        return nodes.size();
    }

    Priv<T> & getPrivateData() {
        return privateData;
    }
};

#endif /* AGGREGATIONTEST_HPP_ */
