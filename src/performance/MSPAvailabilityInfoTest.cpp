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

#include <sstream>
#include "AggregationTest.hpp"
#include "MSPAvailabilityInformation.hpp"
#include "MSPScheduler.hpp"

using stars::TaskProxy;
using stars::LAFunction;
using stars::MSPAvailabilityInformation;

template<> struct Priv<MSPAvailabilityInformation> {
    LAFunction totalAvail;
    LAFunction maxAvail;
};


namespace {
LAFunction dummy;


double createRandomQueue(unsigned int maxmem, unsigned int maxdisk, double power, boost::random::mt19937 & gen, TaskProxy::List & proxys, vector<double> & lBounds) {
    static unsigned int id = 0;
    Time now = Time::getCurrentTime();
    proxys.clear();

    // Add a random number of applications, with random length and number of tasks
    for(int appid = 0; boost::random::uniform_int_distribution<>(1, 3)(gen) != 1; ++appid) {
        double r = boost::random::uniform_int_distribution<>(-1000, 0)(gen);
        unsigned int numTasks = boost::random::uniform_int_distribution<>(1, 10)(gen);
        // Applications between 1-4h on a 1000 MIPS computer
        int a = boost::random::uniform_int_distribution<>(600000, 14400000)(gen) / numTasks;
        for (unsigned int taskid = 0; taskid < numTasks; ++taskid) {
            proxys.push_back(TaskProxy(a, power, now + Duration(r)));
            proxys.back().id = id++;
        }
    }

    if (!proxys.empty()) {
        proxys.getSwitchValues(lBounds);
        proxys.sortMinSlowness(lBounds);
        return proxys.getSlowness();
    } else return 0.0;
}
}


template<> boost::shared_ptr<MSPAvailabilityInformation> AggregationTest<MSPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    boost::shared_ptr<MSPAvailabilityInformation> s(new MSPAvailabilityInformation);
    TaskProxy::List proxys;
    vector<double> lBounds;
    double minSlowness = createRandomQueue(n.mem, n.disk, n.power, gen, proxys, lBounds);
    s->setAvailability(n.mem, n.disk, proxys, lBounds, n.power, minSlowness);
    const LAFunction & maxL = s->getSummary()[0].maxL;
    if (privateData.maxAvail == LAFunction()) {
        privateData.maxAvail = maxL;
    } else {
        privateData.maxAvail.max(privateData.maxAvail, maxL);
        //privateData.minAvail.reduce();
    }
    privateData.totalAvail.maxDiff(dummy, dummy, 1, 1, maxL, privateData.totalAvail);
    return s;
}


class Field {
    ostringstream oss;
    unsigned int width, written;
public:
    Field(unsigned int w) : width(w), written(0) {}
    template<class T> Field & operator<<(const T & t) {
        oss << t;
        written = oss.tellp();
        return *this;
    }
    friend ostream & operator<<(ostream & os, const Field & f) {
        os << f.oss.str();
        if (f.written < f.width)
            os << string(f.width - f.written, ' ');
        return os;
    }
};


void performanceTest(const std::vector<int> & numClusters, int levels) {
    ClusteringVector<MSPAvailabilityInformation::MDLCluster>::setDistVectorSize(20);
    unsigned int numpoints = 8;
    LAFunction::setNumPieces(numpoints);
    ofstream ofmd("msp_mem_disk_slowness.stat");
    for (int j = 0; j < numClusters.size(); j++) {
        MSPAvailabilityInformation::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        LogMsg("Progress", WARN) << "Testing with " << numClusters[j] << " clusters";
        AggregationTest<MSPAvailabilityInformation> t;
        for (int i = 0; i < levels; i++) {
            LogMsg("Progress", WARN) << i << " levels";
            boost::shared_ptr<MSPAvailabilityInformation> result = t.test(i);

            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            LAFunction maxAvail, aggrAvail;
            LAFunction & totalAvail = const_cast<LAFunction &>(t.getPrivateData().totalAvail);
            maxAvail.maxDiff(t.getPrivateData().maxAvail, dummy, t.getNumNodes(), t.getNumNodes(), dummy, dummy);

            unsigned long int aggrMem = 0, aggrDisk = 0;
            {
                const ClusteringVector<MSPAvailabilityInformation::MDLCluster> & clusters = result->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const MSPAvailabilityInformation::MDLCluster & u = clusters[j];
                    aggrMem += (unsigned long int)u.minM * u.value;
                    aggrDisk += (unsigned long int)u.minD * u.value;
                    aggrAvail.maxDiff(u.maxL, dummy, u.value, u.value, aggrAvail, dummy);
                }
            }


            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "M," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << aggrMem << ',' << ((aggrMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << endl;
            ofmd << "D," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << aggrDisk << ',' << ((aggrDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << endl;
            double meanAccuracy = 0.0;
            {
                // TODO: The accuracy is not linear...
                double prevAccuracy = 100.0;
                uint64_t prevA = LAFunction::minTaskLength;
                double ah = totalAvail.getHorizon() * 1.2;
                uint64_t astep = (ah - LAFunction::minTaskLength) / 100;
                for (uint64_t a = LAFunction::minTaskLength; a < ah; a += astep) {
                    double maxAvailBeforeIt = maxAvail.getSlowness(a);
                    double totalAvailBeforeIt = maxAvailBeforeIt - totalAvail.getSlowness(a);
                    double aggrAvailBeforeIt = maxAvailBeforeIt - aggrAvail.getSlowness(a);
                    double accuracy = totalAvailBeforeIt > 0 ? (aggrAvailBeforeIt * 100.0) / totalAvailBeforeIt : 0.0;
                    if (totalAvailBeforeIt + 1 < aggrAvailBeforeIt)
                        LogMsg("test", ERROR) << numClusters[j] << " clusters, total availability is lower than aggregated... (" << totalAvailBeforeIt << " < " << aggrAvailBeforeIt << ')';
                    meanAccuracy += (prevAccuracy + accuracy) * (a - prevA);
                    prevAccuracy = accuracy;
                    prevA = a;
                }

                meanAccuracy /= 2.0 * (ah - LAFunction::minTaskLength);
            }
            ofmd << "L," << (i + 1) << ',' << numClusters[j] << ',' << 0.0 << ',' << 0.0 << ',' << 0.0 << ',' << meanAccuracy << endl;
            ofmd << "s," << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}
