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

#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Logger.hpp"
#include "BasicMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "TrafficStatistics.hpp"
#include "StructureNode.hpp"
#include "StarsNode.hpp"
#include "Scheduler.hpp"
#include "Task.hpp"
#include "TaskDescription.hpp"
#include "Simulator.hpp"
#include "NetworkInterface.hpp"
using namespace std;
using namespace boost::posix_time;
using std::static_pointer_cast;


void TrafficStatistics::msgReceivedAtLevel(unsigned int level, unsigned int size, const std::string & name) {
    if (typeStatsPerLevel.size() <= level)
        typeStatsPerLevel.resize(level + 1);
    typeStatsPerLevel[level][name].received.addMessage(size);
}


void TrafficStatistics::msgSentAtLevel(unsigned int level, unsigned int size, const std::string & name) {
    if (typeStatsPerLevel.size() <= level)
        typeStatsPerLevel.resize(level + 1);
    typeStatsPerLevel[level][name].sent.addMessage(size);
}


void TrafficStatistics::saveTotalStatistics() {
    Simulator & sim = Simulator::getInstance();
    fs::ofstream os(sim.getResultDir() / fs::path("traffic.stat"));
    os << setprecision(6) << fixed;

    unsigned int addr = 0;
    double totalTime = sim.getCurrentTime().getRawDate() / 1000000.0;
    os << "#Node, Level, Bytes sent, fr. of max, Max1sec, fr. of max1sec, Max10sec, fr. of max10sec, data bytes sent, Bytes recv, fr. of max, Max1sec, fr. of max1sec, Max10sec, fr. of max10sec, data bytes recv" << endl;
    for (unsigned int n = 0; n < sim.getNumNodes(); ++n) {
        const stars::NetworkInterface & iface = sim.getNode(n).getNetInterface();
        unsigned int level = sim.getNode(addr).getBranchLevel();
        os << CommAddress(addr, ConfigurationManager::getInstance().getPort()) << ',' << level << ',';
        iface.output(os, totalTime);
        os << endl;
    }
    // End this index
    os << endl << endl;

    os << "# Statistics by message type and level" << endl;
    os << "# Level, msg name, sent msgs, sent bytes, min sent size, max sent size, recv msgs, recv bytes, min recv size, max recv size" << endl;
    for (unsigned int level = 0; level < typeStatsPerLevel.size(); level++) {
        auto & typeLevelStats = typeStatsPerLevel[level];
        for (auto & it : typeLevelStats) {
            LevelStats & mt = it.second;
            os << level << ',' << it.first << ',' << it.second.sent << ',' << it.second.received << endl;
        }
    }
    os << endl << endl;

//    {
//        // Data traffic mean
//        double meanDataSent = 0.0, meanDataRecv = 0.0;
//        double meanControlSent = 0.0, meanControlRecv = 0.0;
//        double numNodes = nodeStatistics.size();
//        for (vector<NodeTraffic>::const_iterator i = nodeStatistics.begin(); i != nodeStatistics.end(); i++) {
//            meanDataSent += i->sent.dataBytes / numNodes;
//            meanDataRecv += i->received.dataBytes / numNodes;
//            meanControlSent += i->sent.bytes / numNodes;
//            meanControlRecv += i->received.bytes / numNodes;
//        }
//        os << "Mean control and data bandwidth (sent/received):" << endl;
//        os << "  Control traffic: " << setprecision(15) << meanControlSent << '/' << meanControlRecv << endl;
//        os << "  Data traffic: " << setprecision(15) << meanDataSent << '/' << meanDataRecv << endl;
//    }
}
