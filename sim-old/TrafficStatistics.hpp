/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
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

#ifndef TRAFFICSTATISTICS_H_
#define TRAFFICSTATISTICS_H_

#include <map>
#include <string>
#include <vector>
#include <list>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Simulator.hpp"

class TrafficStatistics : public Simulator::InterEventHandler {

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

public:
	TrafficStatistics() {
		nodeStatistics.resize(sim.getNumNodes());
	}
	~TrafficStatistics();

	void beforeEvent(const Simulator::Event & ev);
};

#endif /*TRAFFICSTATISTICS_H_*/
