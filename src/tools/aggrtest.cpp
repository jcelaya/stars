/*
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

#include <cmath>
#include <list>
#include <sstream>
#include <fstream>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "IBPAvailabilityInformation.hpp"
#include "MMPAvailabilityInformation.hpp"
#include "DPAvailabilityInformation.hpp"
namespace pt = boost::posix_time;
using namespace boost;
using namespace std;

template<class T> struct Priv;

template<class T> class AggregationTest {
    unsigned int fanout;

    struct Node {
        unsigned int power;
        unsigned int mem;
        unsigned int disk;
        shared_ptr<T> avail;
        size_t size;
    };
    list<Node> nodes;
    unsigned long int totalPower, totalMem, totalDisk;
    Priv<T> privateData;
    shared_ptr<T> totalInfo;

    typename list<Node>::iterator nextNode;
    unsigned long int bytes;
    unsigned long int messages;
    unsigned int maxSize;
    unsigned int minSize;
    // Progress
    unsigned int totalCalls, numCalls;
    pt::ptime lastProgress;
    pt::time_duration aggregationDuration;

    shared_ptr<T> createInfo(const Node & n);

    shared_ptr<T> newNode() {
        if (nextNode != nodes.end()) {
            recordSize(nextNode->size);
            return (nextNode++)->avail;
        } else {
            nodes.push_back(Node());
            Node & n = nodes.back();
            n.power = uniform(min_power, max_power, step_power);
            n.mem = uniform(min_mem, max_mem, step_mem);
            n.disk = uniform(min_disk, max_disk, step_disk);
            totalPower += n.power;
            totalMem += n.mem;
            totalDisk += n.disk;
            n.avail = createInfo(n);
            n.avail->reduce();
            n.size = measureSize(n.avail);
            return n.avail;
        }
    }

    size_t measureSize(shared_ptr<T> e) {
        ostringstream oss;
        msgpack::packer<ostream> pk(&oss);
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

    shared_ptr<T> aggregateLevel(unsigned int level) {
        shared_ptr<T> result;
        if (level == 0) {
            result.reset(newNode()->clone());
            for (unsigned int i = 1; i < fanout; i++) {
                shared_ptr<T> tmp = newNode();
                pt::ptime start = pt::microsec_clock::universal_time();
                result->join(*tmp);
                aggregationDuration += pt::microsec_clock::universal_time() - start;
            }
        } else {
            result = aggregateLevel(level - 1);
            for (unsigned int i = 1; i < fanout; i++) {
                shared_ptr<T> tmp = aggregateLevel(level - 1);
                pt::ptime start = pt::microsec_clock::universal_time();
                result->join(*tmp);
                aggregationDuration += pt::microsec_clock::universal_time() - start;
            }
        }
        pt::ptime start = pt::microsec_clock::universal_time();
        result->reduce();
        measureSize(result);
        aggregationDuration += pt::microsec_clock::universal_time() - start;

        numCalls++;
        unsigned int p = floor(numCalls * 100.0 / totalCalls);
        pt::ptime now = pt::microsec_clock::universal_time();
        if (lastProgress + pt::seconds(1) < now) {
            lastProgress = now;
            cout << p << '%' << endl;
        }
        return result;
    }

public:
    static const int min_power = 1;
    //static const int min_power = 1000;
    static const int max_power = 3000;
    static const int step_power = 1;
    static const int min_mem = 1;
    //static const int min_mem = 256;
    static const int max_mem = 4096;
    static const int step_mem = 1;
    static const int min_disk = 1;
    //static const int min_disk = 500;
    static const int max_disk = 5000;
    static const int step_disk = 1;

    // Return a random int in interval [min, max]
    static int uniform(int min, int max, int step = 1) {
        return min + step * ((int)ceil(floor((double)(max - min) / (double)step + 1.0) * ((rand() + 1.0) / (RAND_MAX + 1.0))) - 1);
    }

    AggregationTest(unsigned int f = 2) : fanout(f), totalPower(0), totalMem(0), totalDisk(0) {
        totalInfo.reset(new T());
    }

    shared_ptr<T> test(unsigned int numLevels) {
        nextNode = nodes.begin();
        messages = 0;
        maxSize = 0;
        minSize = 1000000000;
        bytes = 0;
        totalCalls = (pow((double)fanout, (int)(numLevels + 1)) - 1) / (fanout - 1);
        numCalls = 0;
        aggregationDuration = pt::seconds(0);
        lastProgress = pt::microsec_clock::universal_time();
        return aggregateLevel(numLevels);
    }

    unsigned int getMinSize() const {
        return minSize;
    }

    unsigned int getMaxSize() const {
        return maxSize;
    }

    double getMeanSize() const {
        return (double)bytes / messages;
    }

    pt::time_duration getMeanTime() const {
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

    shared_ptr<T> getTotalInformation() const {
        shared_ptr<T> result(totalInfo->clone());
        result->reduce();
        return result;
    }
};


/******************************/
/* IBPAvailabilityInformation */
/******************************/
template<> struct Priv<IBPAvailabilityInformation> {};


template<> shared_ptr<IBPAvailabilityInformation> AggregationTest<IBPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    shared_ptr<IBPAvailabilityInformation> result(new IBPAvailabilityInformation);
    result->addNode(n.mem, n.disk);
    totalInfo->addNode(n.mem, n.disk);
    return result;
}


void ibpAggr(int numLevels, int numValues, int * numClusters) {
    ofstream ofmd("ibp_aggr_mem_disk.stat");

    for (int j = 0; j < numValues; ++j) {
        IBPAvailabilityInformation::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        AggregationTest<IBPAvailabilityInformation> t;
        for (int i = 0; i < numLevels; ++i) {
            list<IBPAvailabilityInformation::MDCluster *> clusters;
            TaskDescription dummy;
            dummy.setMaxMemory(0);
            dummy.setMaxDisk(0);
            shared_ptr<IBPAvailabilityInformation> result = t.test(i);
            result->getAvailability(clusters, dummy);
            // Do not calculate total information and then aggregate, it is not very useful
            unsigned long int aggrMem = 0, aggrDisk = 0;
            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            for (list<IBPAvailabilityInformation::MDCluster *>::iterator it = clusters.begin(); it != clusters.end(); it++) {
                aggrMem += (unsigned long int)(*it)->minM * (*it)->value;
                aggrDisk += (unsigned long int)(*it)->minD * (*it)->value;
            }
            cout << t.getNumNodes() << " nodes, " << numClusters[j] << " s.f., "
                    << t.getMeanTime().total_microseconds() << " us/msg, "
                    << "min/mean/max size " << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()
                    << " mem " << aggrMem << '/' << t.getTotalMem() << '(' << (aggrMem * 100.0 / t.getTotalMem()) << "%)"
                    << " disk " << aggrDisk << '/' << t.getTotalDisk() << '(' << (aggrDisk * 100.0 / t.getTotalDisk()) << "%)" << endl;

            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << (minMem * 100.0 / t.getTotalMem()) << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << (minDisk * 100.0 / t.getTotalDisk()) << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}


/******************************/
/* MMPAvailabilityInformation */
/******************************/
template<> struct Priv<MMPAvailabilityInformation> {
    Duration maxQueue;
    Duration totalQueue;
};


static Time reference = Time::getCurrentTime();


template<> shared_ptr<MMPAvailabilityInformation> AggregationTest<MMPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    static const int min_time = 0;
    static const int max_time = 2000;
    static const int step_time = 1;
    shared_ptr<MMPAvailabilityInformation> result(new MMPAvailabilityInformation);
    Duration q((double)uniform(min_time, max_time, step_time));
    result->setQueueEnd(n.mem, n.disk, n.power, reference + q);
    totalInfo->setQueueEnd(n.mem, n.disk, n.power, reference + q);
    if (privateData.maxQueue < q)
        privateData.maxQueue = q;
    privateData.totalQueue += q;
    return result;
}


void mmpAggr(int numLevels, int numValues, int * numClusters) {
    ofstream ofmd("mmp_aggr_mem_disk_power.stat");

    for (int j = 0; j < numValues; j++) {
        MMPAvailabilityInformation::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        AggregationTest<MMPAvailabilityInformation> t;
        for (int i = 0; i < numLevels; i++) {
            list<MMPAvailabilityInformation::MDPTCluster *> clusters;
            TaskDescription dummy;
            dummy.setMaxMemory(0);
            dummy.setMaxDisk(0);
            dummy.setLength(1);
            dummy.setDeadline(Time::getCurrentTime() + Duration(10000.0));
            shared_ptr<MMPAvailabilityInformation> result = t.test(i);
            result->getAvailability(clusters, dummy);
            // Do not calculate total information and then aggregate, it is not very useful
            unsigned long int aggrMem = 0, aggrDisk = 0, aggrPower = 0;
            Duration aggrQueue;
            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            unsigned long int minPower = t.getNumNodes() * t.min_power;
            Duration maxQueue = t.getPrivateData().maxQueue * t.getNumNodes();
            Duration totalQueue = maxQueue - t.getPrivateData().totalQueue;
            for (list<MMPAvailabilityInformation::MDPTCluster *>::iterator it = clusters.begin(); it != clusters.end(); it++) {
                aggrMem += (unsigned long int)(*it)->minM * (*it)->value;
                aggrDisk += (unsigned long int)(*it)->minD * (*it)->value;
                aggrPower += (unsigned long int)(*it)->minP * (*it)->value;
                aggrQueue += (t.getPrivateData().maxQueue - ((*it)->maxT - reference)) * (*it)->value;
            }
            cout << t.getNumNodes() << " nodes, " << numClusters[j] << " s.f., "
                    << t.getMeanTime().total_microseconds() << " us/msg, "
                    << "min/mean/max size " << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()
                    << " mem " << aggrMem << " / " << t.getTotalMem() << " = " << (aggrMem * 100.0 / t.getTotalMem()) << "%"
                    << " disk " << aggrDisk << " / " << t.getTotalDisk() << " = " << (aggrDisk * 100.0 / t.getTotalDisk()) << "%"
                    << " power " << aggrPower << " / " << t.getTotalPower() << " = " << (aggrPower * 100.0 / t.getTotalPower()) << "%"
                    << " queue " << aggrQueue.seconds() << " / " << totalQueue.seconds() << " = " << (aggrQueue.seconds() * 100.0 / totalQueue.seconds()) << "%" << endl;

            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalPower() << ',' << minPower << ',' << aggrPower << ',' << (aggrPower * 100.0 / t.getTotalPower()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << totalQueue.seconds() << ',' << maxQueue.seconds() << ',' << aggrQueue.seconds() << ',' << (aggrQueue.seconds() * 100.0 / totalQueue.seconds()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}


/*****************************/
/* DPAvailabilityInformation */
/*****************************/
template<> struct Priv<DPAvailabilityInformation> {
    Time ref;
    DPAvailabilityInformation::ATFunction totalAvail;
    DPAvailabilityInformation::ATFunction minAvail;
};


namespace {
    list<Time> createRandomLAF(double power, Time ct) {
        Time next = ct, h = ct + Duration(100000.0);
        list<Time> result;

        // Add a random number of tasks, with random length
        while(AggregationTest<DPAvailabilityInformation>::uniform(1, 3) != 1) {
            // Tasks of 5-60 minutes on a 1000 MIPS computer
            unsigned long int length = AggregationTest<DPAvailabilityInformation>::uniform(300000, 3600000);
            next += Duration(length / power);
            result.push_back(next);
            // Similar time for holes
            unsigned long int nextHole = AggregationTest<DPAvailabilityInformation>::uniform(300000, 3600000);
            next += Duration(nextHole / power);
            result.push_back(next);
        }
        if (!result.empty()) {
            // set a good horizon
            if (next < h) result.back() = h;
        }

        return result;
    }


    string plot(DPAvailabilityInformation::ATFunction & f) {
        ostringstream os;
        if (f.getPoints().empty()) {
            os << "0,0" << endl;
            os << "100000000000," << (unsigned long)(f.getSlope() * 100000.0) << endl;
        } else {
            for (vector<pair<Time, uint64_t> >::const_iterator it = f.getPoints().begin(); it != f.getPoints().end(); it++)
                os << it->first.getRawDate() << ',' << it->second << endl;
        }
        return os.str();
    }
}


template<> shared_ptr<DPAvailabilityInformation> AggregationTest<DPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    shared_ptr<DPAvailabilityInformation> result(new DPAvailabilityInformation);
    list<Time> q = createRandomLAF(n.power, privateData.ref);
    result->addNode(n.mem, n.disk, n.power, q);
    totalInfo->addNode(n.mem, n.disk, n.power, q);
    const DPAvailabilityInformation::ATFunction & minA = result->getSummary()[0].minA;
    if (privateData.minAvail.getSlope() == 0.0)
        privateData.minAvail = minA;
    else
        privateData.minAvail.min(privateData.minAvail, minA);
    privateData.totalAvail.lc(privateData.totalAvail, minA, 1.0, 1.0);
    return result;
}


Time Time::getCurrentTime() {
    return Time(0);
}


void dpAggr(int numLevels, int numValues, int * numClusters) {
    Time ct = Time::getCurrentTime();
    ClusteringVector<DPAvailabilityInformation::MDFCluster>::setDistVectorSize(20);
    unsigned int numpoints = 10;
    DPAvailabilityInformation::setNumRefPoints(numpoints);
    ofstream off("dp_aggr_deadline.stat"), ofmd("dp_aggr_mem_disk.stat");
    DPAvailabilityInformation::ATFunction dummy;

    for (int j = 0; j < numValues; j++) {
        DPAvailabilityInformation::setNumClusters(numClusters[j]);
        off << "# " << numClusters[j] << " clusters" << endl;
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        AggregationTest<DPAvailabilityInformation> t;
        t.getPrivateData().ref = ct;
        for (int i = 0; i < numLevels; i++) {
            shared_ptr<DPAvailabilityInformation> result = t.test(i);

            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            DPAvailabilityInformation::ATFunction minAvail, aggrAvail, treeAvail;
            DPAvailabilityInformation::ATFunction & totalAvail = const_cast<DPAvailabilityInformation::ATFunction &>(t.getPrivateData().totalAvail);
            minAvail.lc(t.getPrivateData().minAvail, dummy, t.getNumNodes(), 1.0);

            unsigned long int aggrMem = 0, aggrDisk = 0;
            {
                shared_ptr<DPAvailabilityInformation> totalInformation = t.getTotalInformation();
                const ClusteringVector<DPAvailabilityInformation::MDFCluster> & clusters = totalInformation->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const DPAvailabilityInformation::MDFCluster & u = clusters[j];
                    aggrMem += (unsigned long int)u.minM * u.value;
                    aggrDisk += (unsigned long int)u.minD * u.value;
                    aggrAvail.lc(aggrAvail, u.minA, 1.0, u.value);
                }
            }

            unsigned long int treeMem = 0, treeDisk = 0;
            {
                const ClusteringVector<DPAvailabilityInformation::MDFCluster> & clusters = result->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const DPAvailabilityInformation::MDFCluster & u = clusters[j];
                    treeMem += (unsigned long int)u.minM * u.value;
                    treeDisk += (unsigned long int)u.minD * u.value;
                    treeAvail.lc(treeAvail, u.minA, 1.0, u.value);
                }
            }

            cout << t.getNumNodes() << " nodes, " << numClusters[j] << " s.f., "
                    << t.getMeanTime().total_microseconds() << " us/msg, "
                    << "min/mean/max size " << t.getMinSize() << '/' << t.getMeanSize() << '/' << t.getMaxSize()
                    << " mem " << (treeMem - minMem) << '/' << (t.getTotalMem() - minMem) << '(' << ((treeMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "%)"
                    << " disk " << (treeDisk - minDisk) << '/' << (t.getTotalDisk() - minDisk) << '(' << ((treeDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "%)" << endl;
                    //<< " avail " << deltaAggrAvail << '/' << deltaTotalAvail;
            cout << "Full aggregation: "
                    << " mem " << (aggrMem - minMem) << '/' << (t.getTotalMem() - minMem) << '(' << ((aggrMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << "%)"
                    << " disk " << (aggrDisk - minDisk) << '/' << (t.getTotalDisk() - minDisk) << '(' << ((aggrDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << "%)" << endl;
                    //<< " avail " << deltaAggrAvail << '/' << deltaTotalAvail;
            list<Time> p;
            for (vector<pair<Time, uint64_t> >::const_iterator it = aggrAvail.getPoints().begin(); it != aggrAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, uint64_t> >::const_iterator it = treeAvail.getPoints().begin(); it != treeAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, uint64_t> >::const_iterator it = totalAvail.getPoints().begin(); it != totalAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, uint64_t> >::const_iterator it = minAvail.getPoints().begin(); it != minAvail.getPoints().end(); it++)
                p.push_back(it->first);
            p.sort();
            off << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << (minMem * 100.0 / t.getTotalMem()) << ',' << aggrMem << ',' << (aggrMem * 100.0 / t.getTotalMem()) << ',' << treeMem << ',' << (treeMem * 100.0 / t.getTotalMem()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << (minDisk * 100.0 / t.getTotalDisk()) << ',' << aggrDisk << ',' << (aggrDisk * 100.0 / t.getTotalDisk()) << ',' << treeDisk << ',' << (treeDisk * 100.0 / t.getTotalDisk()) << endl;
            ofmd << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            double lastTime = -1.0;
            for (list<Time>::iterator it = p.begin(); it != p.end(); it++) {
                unsigned long t = totalAvail.getAvailabilityBefore(*it),
                             a = aggrAvail.getAvailabilityBefore(*it),
                             at = treeAvail.getAvailabilityBefore(*it),
                             min = minAvail.getAvailabilityBefore(*it);
                             double time = floor((*it - ct).seconds() * 1000.0) / 1000.0;
                             if (lastTime != time) {
                                 off << time << ',' << t
                                 << ',' << min << ',' << (t == 0 ? 100 : min * 100.0 / t)
                                 << ',' << a << ',' << (t == 0 ? 100 : a * 100.0 / t)
                                 << ',' << at << ',' << (t == 0 ? 100 : at * 100.0 / t) << endl;
                                 lastTime = time;
                             }
            }
            off << endl;
            ofmd << endl;
        }
        off << endl;
        ofmd << endl;
    }
    off.close();
    ofmd.close();
}


int main(int argc, char * argv[]) {
    char coma;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == string("-ibp") && ++i < argc) {
            int numLevels = 0;
            istringstream opts(argv[i]);
            opts >> numLevels;
            vector<int> clusters;
            while (opts.good()) {
                int c = 0;
                opts >> coma >> c;
                if (c != 0)
                    clusters.push_back(c);
            }
            ibpAggr(numLevels, clusters.size(), clusters.data());
        }
        else if (argv[i] == string("-mmp") && ++i < argc) {
            int numLevels = 0;
            istringstream opts(argv[i]);
            opts >> numLevels;
            vector<int> clusters;
            while (opts.good()) {
                int c = 0;
                opts >> coma >> c;
                if (c != 0)
                    clusters.push_back(c);
            }
            mmpAggr(numLevels, clusters.size(), clusters.data());
        }
        else if (argv[i] == string("-dp") && ++i < argc) {
            int numLevels = 0;
            istringstream opts(argv[i]);
            opts >> numLevels;
            vector<int> clusters;
            while (opts.good()) {
                int c = 0;
                opts >> coma >> c;
                if (c != 0)
                    clusters.push_back(c);
            }
            dpAggr(numLevels, clusters.size(), clusters.data());
        }
    }
    return 0;
}
