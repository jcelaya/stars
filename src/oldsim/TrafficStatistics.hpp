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

#ifndef TRAFFICSTATISTICS_H_
#define TRAFFICSTATISTICS_H_

#include <map>
#include <string>
#include <vector>
#include <list>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Time.hpp"

class TrafficStatistics {

    struct MessageType {
        unsigned long int numMessages;
        unsigned long int minSize, maxSize, totalBytes;
        MessageType() : numMessages(0), minSize(100000000), maxSize(0), totalBytes(0) {}
    };
    std::map<std::string, MessageType> typeNetStatistics;
    std::map<std::string, MessageType> typeSelfStatistics;
    std::vector<std::map<std::string, MessageType> > typeSentStatistics;
    std::vector<std::map<std::string, MessageType> > typeRecvStatistics;

    struct NodeTraffic {
        unsigned long int bytesSent, dataBytesSent;
        unsigned long int bytesReceived, dataBytesRecv;
        unsigned long int maxBytesIn1sec, maxBytesIn10sec;
        unsigned long int maxBytesOut1sec, maxBytesOut10sec;
        unsigned long int lastBytesIn[2], lastBytesOut[2];
        std::list<std::pair<unsigned long int, Time> > lastSentSizes[2], lastRecvSizes[2];
        NodeTraffic() : bytesSent(0), dataBytesSent(0), bytesReceived(0), dataBytesRecv(0),
            maxBytesIn1sec(0), maxBytesIn10sec(0), maxBytesOut1sec(0), maxBytesOut10sec(0) {
            lastBytesIn[0] = lastBytesIn[1] = 0;
            lastBytesOut[0] = lastBytesOut[1] = 0;
        }
    };
    std::vector<NodeTraffic> nodeStatistics;

    static Duration intervals[2];

public:
    void setNumNodes(unsigned int n) {
        nodeStatistics.resize(n);
    }

    void saveTotalStatistics();

    void msgReceived(uint32_t src, uint32_t dst, unsigned int size, double inBW, const BasicMsg & msg);
    void msgSent(uint32_t src, uint32_t dst, unsigned int size, double outBW, Time finish, const BasicMsg & msg);
};

#endif /*TRAFFICSTATISTICS_H_*/