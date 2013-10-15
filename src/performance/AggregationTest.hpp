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

#include <list>
#include <map>
#include <sstream>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "AvailabilityInformation.hpp"
#include "Logger.hpp"
#include "util/MemoryManager.hpp"
#include "../test/scheduling/RandomQueueGenerator.hpp"
using namespace std;
using namespace boost::posix_time;

template<class T> struct Priv;

class AggregationTest {
public:
    enum {
        min_power = 1000,
        max_power = 3000,
    //    step_power = 1,
        min_mem = 256,
        max_mem = 4096,
    //    step_mem = 1,
        min_disk = 500,
        max_disk = 5000,
    //    step_disk = 1,
    };

    static AggregationTest & getInstance();

    virtual ~AggregationTest() {}

    void performanceTest(const std::vector<int> & numClusters, int levels)  {
        for (int j = 0; j < numClusters.size(); j++) {
            of << "# " << numClusters[j] << " clusters" << endl;
            Logger::msg("Progress", WARN, "Testing with ", numClusters[j], " clusters");
            reset(numClusters[j]);
            unsigned long int initialMemory = MemoryManager::getInstance().getUsedMemory();
            for (int i = 0; i < levels; i++) {
                Logger::msg("Progress", WARN, i, " levels");
                test(i);
                Logger::msg("Progress", WARN, i, " levels used ", (MemoryManager::getInstance().getUsedMemory() - initialMemory), " bytes.");

                of << "# " << (i + 1) << " levels, " << getNumNodes() << " nodes" << endl;
                for (auto & r: results) {
                    of << r.first << ',' << (i + 1) << ',' << numClusters[j];
                    for (auto & v: r.second) {
                        of << ',' << v;
                    }
                    of << endl;
                }
                of << "s," << (i + 1) << ',' << numClusters[j] << ',' << minSize << ',' << getMeanSize() << ',' << maxSize << ',' << getMeanTime().total_microseconds() << endl;
                of << endl;
            }
            of << endl;
        }
        of.close();
    }

protected:
    unsigned int fanout;

    unsigned long int totalPower, totalMem, totalDisk;
    stars::RandomQueueGenerator gen;
    boost::random::uniform_int_distribution<> unifPower, unifMemory, unifDisk;
    unsigned long int bytes;
    unsigned long int messages;
    unsigned int maxSize;
    unsigned int minSize;
    // Progress
    unsigned int totalCalls, numCalls;
    ptime lastProgress;
    time_duration aggregationDuration;
    // Results
    class ValueList : public std::list<double> {
    public:
        ValueList & value(double v) {
            push_back(v);
            return *this;
        }
    };
    std::map<std::string, ValueList> results;
    std::ofstream of;

    virtual void reset(int numClusters) = 0;

    virtual void test(unsigned int numLevels) = 0;

    unsigned int getMeanSize() const {
        return bytes / messages;
    }

    time_duration getMeanTime() const {
        return aggregationDuration / (messages / 2);
    }

    virtual size_t getNumNodes() const = 0;

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

    AggregationTest(const char * filename, unsigned int f = 2) : fanout(f), totalPower(0), totalMem(0), totalDisk(0),
            unifPower(min_power, max_power), unifMemory(min_mem, max_mem), unifDisk(min_disk, max_disk), of(filename) {
        // gen.seed(12354);
        of.precision(10);
    }

};


template<class T> class AggregationTestImpl : public AggregationTest {
public:
    AggregationTestImpl();

    virtual ~AggregationTestImpl() {}

private:
    Priv<T> privateData;

    struct Node {
        unsigned int power;
        unsigned int mem;
        unsigned int disk;
        boost::shared_ptr<T> avail;
        size_t size;
    };
    list<Node> nodes;
    typename list<Node>::iterator nextNode;

    virtual void reset(int numClusters) {
        T::setNumClusters(numClusters);
        privateData = Priv<T>();
        nodes.clear();
        totalPower = totalMem = totalDisk = 0;
    }

    virtual void test(unsigned int numLevels) {
        results.clear();
        nextNode = nodes.begin();
        messages = 0;
        maxSize = 0;
        minSize = 1000000000;
        bytes = 0;
        totalCalls = (pow((double)fanout, (int)(numLevels + 1)) - 1) / (fanout - 1);
        numCalls = 0;
        aggregationDuration = seconds(0);
        lastProgress = microsec_clock::universal_time();
        computeResults(aggregateLevel(numLevels));
    }

    virtual size_t getNumNodes() const {
        return nodes.size();
    }

    boost::shared_ptr<T> createInfo(const Node & n);

    void computeResults(const boost::shared_ptr<T> & summary);

    boost::shared_ptr<T> newNode() {
        if (nextNode != nodes.end()) {
            recordSize(nextNode->size);
            return (nextNode++)->avail;
        } else {
            nodes.push_back(Node());
            Node & n = nodes.back();
            n.power = unifPower(gen.getGenerator()); //floor(unifPower(gen) / step_power) * step_power;
            n.mem = unifMemory(gen.getGenerator()); //floor(unifMemory(gen) / step_mem) * step_mem;
            n.disk = unifDisk(gen.getGenerator()); //floor(unifDisk(gen) / step_disk) * step_disk;
            totalPower += n.power;
            totalMem += n.mem;
            totalDisk += n.disk;
            n.avail = createInfo(n);
            n.avail->reduce();
            n.size = measureSize(n.avail);
            return n.avail;
        }
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
            Logger::msg("Progress", WARN, p, '%');
        }
        return result;
    }

};

#endif /* AGGREGATIONTEST_HPP_ */
