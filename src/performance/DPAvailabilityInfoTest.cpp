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

#include "AggregationTest.hpp"
#include "DPAvailabilityInformation.hpp"
//using namespace boost;


template<> struct Priv<DPAvailabilityInformation> {
    DPAvailabilityInformation::ATFunction totalAvail;
    DPAvailabilityInformation::ATFunction minAvail;
};


namespace {
    Time refTime;

    void createRandomLAF(double power, boost::random::mt19937 & gen, list<Time> & result) {
        Time next = refTime, h = refTime + Duration(100000.0);

        // Add a random number of tasks, with random length
        while(boost::random::uniform_int_distribution<>(1, 3)(gen) != 1) {
            // Tasks of 5-60 minutes on a 1000 MIPS computer
            unsigned long int length = boost::random::uniform_int_distribution<>(300000, 3600000)(gen);
            next += Duration(length / power);
            result.push_back(next);
            // Similar time for holes
            unsigned long int nextHole = boost::random::uniform_int_distribution<>(300000, 3600000)(gen);
            next += Duration(nextHole / power);
            result.push_back(next);
        }
        if (!result.empty()) {
            // set a good horizon
            if (next < h) result.back() = h;
        }
    }
}


template<> boost::shared_ptr<DPAvailabilityInformation> AggregationTest<DPAvailabilityInformation>::createInfo(const AggregationTest::Node & n) {
    boost::shared_ptr<DPAvailabilityInformation> result(new DPAvailabilityInformation);
    list<Time> q;
    createRandomLAF(n.power, gen, q);
    result->addNode(n.mem, n.disk, n.power, q);
    const DPAvailabilityInformation::ATFunction & minA = result->getSummary()[0].minA;
    if (privateData.minAvail.getSlope() == 0.0)
        privateData.minAvail = minA;
    else
        privateData.minAvail.min(privateData.minAvail, minA);
    privateData.totalAvail.lc(privateData.totalAvail, minA, 1.0, 1.0);
    return result;
}


void performanceTest(const std::vector<int> & numClusters, int levels) {
    refTime = Time::getCurrentTime();
    ClusteringVector<DPAvailabilityInformation::MDFCluster>::setDistVectorSize(20);
    unsigned int numpoints = 10;
    DPAvailabilityInformation::setNumRefPoints(numpoints);
    ofstream ofmd("dp_mem_disk_avail.stat");
    DPAvailabilityInformation::ATFunction dummy;

    for (int j = 0; j < numClusters.size(); j++) {
        DPAvailabilityInformation::setNumClusters(numClusters[j]);
        ofmd << "# " << numClusters[j] << " clusters" << endl;
        LogMsg("Progress", WARN) << "Testing with " << numClusters[j] << " clusters";
        AggregationTest<DPAvailabilityInformation> t;
        for (int i = 0; i < levels; i++) {
            LogMsg("Progress", WARN) << i << " levels";
            boost::shared_ptr<DPAvailabilityInformation> result = t.test(i);

            unsigned long int minMem = t.getNumNodes() * t.min_mem;
            unsigned long int minDisk = t.getNumNodes() * t.min_disk;
            DPAvailabilityInformation::ATFunction minAvail, aggrAvail;
            DPAvailabilityInformation::ATFunction & totalAvail = const_cast<DPAvailabilityInformation::ATFunction &>(t.getPrivateData().totalAvail);
            minAvail.lc(t.getPrivateData().minAvail, dummy, t.getNumNodes(), 1.0);

            unsigned long int aggrMem = 0, aggrDisk = 0;
            {
                const ClusteringVector<DPAvailabilityInformation::MDFCluster> & clusters = result->getSummary();
                for (size_t j = 0; j < clusters.getSize(); j++) {
                    const DPAvailabilityInformation::MDFCluster & u = clusters[j];
                    aggrMem += (unsigned long int)u.minM * u.value;
                    aggrDisk += (unsigned long int)u.minD * u.value;
                    aggrAvail.lc(aggrAvail, u.minA, 1.0, u.value);
                }
            }

            list<Time> p;
            for (vector<pair<Time, double> >::const_iterator it = aggrAvail.getPoints().begin(); it != aggrAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, double> >::const_iterator it = totalAvail.getPoints().begin(); it != totalAvail.getPoints().end(); it++)
                p.push_back(it->first);
            for (vector<pair<Time, double> >::const_iterator it = minAvail.getPoints().begin(); it != minAvail.getPoints().end(); it++)
                p.push_back(it->first);
            p.sort();
            p.erase(std::unique(p.begin(), p.end()), p.end());
            ofmd << "# " << (i + 1) << " levels, " << t.getNumNodes() << " nodes" << endl;
            ofmd << "M," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalMem() << ',' << minMem << ',' << aggrMem << ',' << ((aggrMem - minMem) * 100.0 / (t.getTotalMem() - minMem)) << endl;
            ofmd << "D," << (i + 1) << ',' << numClusters[j] << ',' << t.getTotalDisk() << ',' << minDisk << ',' << aggrDisk << ',' << ((aggrDisk - minDisk) * 100.0 / (t.getTotalDisk() - minDisk)) << endl;
            double meanAccuracy = 0.0;
            {
                double prevAccuracy = 0.0;
                Time prevTime = refTime;
                // Approximate the accuracy to linear...
                for (list<Time>::iterator it = p.begin(); it != p.end(); ++it) {
                    double minAvailBeforeIt = minAvail.getAvailabilityBefore(*it);
                    double totalAvailBeforeIt = totalAvail.getAvailabilityBefore(*it)- minAvailBeforeIt;
                    double aggrAvailBeforeIt = aggrAvail.getAvailabilityBefore(*it)- minAvailBeforeIt;
                    double accuracy = totalAvailBeforeIt > minAvailBeforeIt ? (aggrAvailBeforeIt * 100.0) / totalAvailBeforeIt : 0.0;
                    if (totalAvailBeforeIt + 1 < aggrAvailBeforeIt)
                        LogMsg("test", ERROR) << numClusters[j] << " clusters, total availability is lower than aggregated... (" << totalAvailBeforeIt << " < " << aggrAvailBeforeIt << ')';
                    //ofmd << "#," << *it << ',' << totalAvailBeforeIt << ',' << aggrAvailBeforeIt << ',' << accuracy << endl;
                    meanAccuracy += (prevAccuracy + accuracy) * (*it - prevTime).seconds();
                    prevAccuracy = accuracy;
                    prevTime = *it;
                }
                meanAccuracy /= 2.0 * (p.back() - refTime).seconds();
            }
            ofmd << "A," << (i + 1) << ',' << numClusters[j] << ',' << 0.0 << ',' << 0.0 << ',' << 0.0 << ',' << meanAccuracy << endl;
            ofmd << "s," << (i + 1) << ',' << numClusters[j] << ',' << t.getMeanSize() << ',' << t.getMeanTime().total_microseconds() << endl;
            ofmd << endl;
        }
        ofmd << endl;
    }
    ofmd.close();
}
