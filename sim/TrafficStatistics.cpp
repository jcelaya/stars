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

#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include "Logger.hpp"
#include "BasicMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "TrafficStatistics.hpp"
#include "StructureNode.hpp"
#include "PeerCompNode.hpp"
#include "Scheduler.hpp"
#include "Task.hpp"
#include "TaskDescription.hpp"
#include "Distributions.hpp"
#include "Simulator.hpp"
using namespace std;
using namespace boost::posix_time;
using boost::static_pointer_cast;


void TrafficStatistics::beforeEvent(const Simulator::Event & ev) {
	// Message type statistics
	{
		MessageType & mt = ev.from == ev.to ? typeSelfStatistics[ev.msg->getName()] : typeNetStatistics[ev.msg->getName()];
		mt.numMessages++;
		mt.totalBytes += ev.size;
		if (mt.maxSize < ev.size) mt.maxSize = ev.size;
		if (mt.minSize > ev.size) mt.minSize = ev.size;
	}

	// If it is a finishing task, account for data transfers
	if (typeid(*ev.msg) == typeid(TaskStateChgMsg)) {
		NodeTraffic & nt = nodeStatistics[ev.to];
		TaskDescription & desc = sim.getNode(ev.from).getScheduler()
				.getTask(static_pointer_cast<TaskStateChgMsg>(ev.msg)->getTaskId())->getDescription();
		nt.dataBytesRecv += desc.getInputSize() * 1024ULL;
		nt.dataBytesSent += desc.getOutputSize() * 1024ULL;
	}

	// Next statistics only apply with network messages
	if (ev.from == ev.to) return;

	{
		unsigned int level = sim.getNode(ev.from).getSNLevel();
		if (typeSentStatistics.size() <= level) typeSentStatistics.resize(level + 1);
		if (typeRecvStatistics.size() <= level) typeRecvStatistics.resize(level + 1);
		MessageType & mt = typeSentStatistics[level][ev.msg->getName()];
		mt.numMessages++;
		mt.totalBytes += ev.size;
		if (mt.maxSize < ev.size) mt.maxSize = ev.size;
		if (mt.minSize > ev.size) mt.minSize = ev.size;
		typeRecvStatistics[level][ev.msg->getName()];
	}
	{
		unsigned int level = sim.getNode(ev.to).getSNLevel();
		if (typeSentStatistics.size() <= level) typeSentStatistics.resize(level + 1);
		if (typeRecvStatistics.size() <= level) typeRecvStatistics.resize(level + 1);
		MessageType & mt = typeRecvStatistics[level][ev.msg->getName()];
		mt.numMessages++;
		mt.totalBytes += ev.size;
		if (mt.maxSize < ev.size) mt.maxSize = ev.size;
		if (mt.minSize > ev.size) mt.minSize = ev.size;
		typeSentStatistics[level][ev.msg->getName()];
	}

	// Node traffic statistics
	static Duration intervals[2] = {Duration(1.0), Duration(10.0)};
	// First, in traffic
	{
		NodeTraffic & nt = nodeStatistics[ev.to];
		nt.bytesReceived += ev.size;
		for (int i = 0; i < 2; i++) {
			nt.lastRecvSizes[i].push_back(make_pair(ev.size, ev.t - ev.txDuration));
			nt.lastBytesIn[i] += ev.size;
			while (ev.t - nt.lastRecvSizes[i].front().second > intervals[i]) {
				nt.lastBytesIn[i] -= nt.lastRecvSizes[i].front().first;
				nt.lastRecvSizes[i].pop_front();
			}
		}
		if (nt.maxBytesIn1sec < nt.lastBytesIn[0]) nt.maxBytesIn1sec = nt.lastBytesIn[0];
		if (nt.maxBytesIn10sec < nt.lastBytesIn[1]) nt.maxBytesIn10sec = nt.lastBytesIn[1];
	}

	// Now, out traffic
	{
		NodeTraffic & nt = nodeStatistics[ev.from];
		nt.bytesSent += ev.size;
		Time finish = ev.txTime + ev.txDuration;
		for (int i = 0; i < 2; i++) {
			nt.lastSentSizes[i].push_back(make_pair(ev.size, ev.txTime));
			nt.lastBytesOut[i] += ev.size;
			while (finish - nt.lastSentSizes[i].front().second > intervals[i]) {
				nt.lastBytesOut[i] -= nt.lastSentSizes[i].front().first;
				nt.lastSentSizes[i].pop_front();
			}
		}
		if (nt.maxBytesOut1sec < nt.lastBytesOut[0]) nt.maxBytesOut1sec = nt.lastBytesOut[0];
		if (nt.maxBytesOut10sec < nt.lastBytesOut[1]) nt.maxBytesOut10sec = nt.lastBytesOut[1];
	}
}


TrafficStatistics::~TrafficStatistics() {
	fs::ofstream os(sim.getResultDir() / fs::path("traffic.stat"));
	os << setprecision(6) << fixed;

	double maxSent = 0.0, maxOut1s = 0.0, maxOut10s = 0.0, maxRecv = 0.0, maxIn1s = 0.0, maxIn10s = 0.0;
	unsigned int maxLevel = 0;
	unsigned int addr = 0;
	double totalTime = sim.getCurrentTime().getRawDate() / 1000000.0;
	os << "#Node, Level, Bytes sent, fr. of max, Max1sec, fr. of max1sec, Max10sec, fr. of max10sec, Bytes recv, fr. of max, Max1sec, fr. of max1sec, Max10sec, fr. of max10sec, data bytes sent, data bytes recv" << endl;
	for (vector<NodeTraffic>::const_iterator i = nodeStatistics.begin(); i != nodeStatistics.end(); i++, addr++) {
		double inBW = sim.getNetInterface(addr).inBW;
		double outBW = sim.getNetInterface(addr).outBW;
		unsigned int level = sim.getNode(addr).getSNLevel();
		if (level > maxLevel) maxLevel = level;
		os << CommAddress(addr, ConfigurationManager::getInstance().getPort()) << ',' << level << ','
			<< i->bytesSent << ',' << ((i->bytesSent / totalTime) / outBW) << ','
			<< i->maxBytesOut1sec << ',' << (i->maxBytesOut1sec / outBW) << ','
			<< (i->maxBytesOut10sec / 10) << ',' << ((i->maxBytesOut10sec / 10.0) / outBW) << ','
			<< i->bytesReceived << ',' << ((i->bytesReceived / totalTime) / inBW) << ','
			<< i->maxBytesIn1sec << ',' << (i->maxBytesIn1sec / inBW) << ','
			<< (i->maxBytesIn10sec / 10) << ',' << ((i->maxBytesIn10sec / 10.0) / inBW) << ','
			<< i->dataBytesSent << ',' << i->dataBytesRecv
			<< endl;
		if ((i->bytesSent / totalTime) / outBW > maxSent) maxSent = (i->bytesSent / totalTime) / outBW;
		if (i->maxBytesOut1sec / outBW > maxOut1s) maxOut1s = i->maxBytesOut1sec / outBW;
		if ((i->maxBytesOut10sec / 10.0) / outBW > maxOut10s) maxOut10s = (i->maxBytesOut10sec / 10.0) / outBW;
		if ((i->bytesReceived / totalTime) / inBW > maxRecv) maxRecv = (i->bytesReceived / totalTime) / inBW;
		if (i->maxBytesIn1sec / inBW > maxIn1s) maxIn1s = i->maxBytesIn1sec / inBW;
		if ((i->maxBytesIn10sec / 10.0) / inBW > maxIn10s) maxIn10s = (i->maxBytesIn10sec / 10.0) / inBW;
	}
	// End this index
	os << endl << endl;

	// Next blocks are CDF of each fraction value, and every level
	vector<Histogram> sentFraction(maxLevel + 1, Histogram(maxSent / 100.0)),
		maxOut1sFraction(maxLevel + 1, Histogram(maxOut1s / 100.0)),
		maxOut10sFraction(maxLevel + 1, Histogram(maxOut10s / 100.0)),
		recvFraction(maxLevel + 1, Histogram(maxRecv / 100.0)),
		maxIn1sFraction(maxLevel + 1, Histogram(maxIn1s / 100.0)),
		maxIn10sFraction(maxLevel + 1, Histogram(maxIn10s / 100.0));
	addr = 0;
	for (vector<NodeTraffic>::const_iterator i = nodeStatistics.begin(); i != nodeStatistics.end(); i++, addr++) {
		double inBW = sim.getNetInterface(addr).inBW;
		double outBW = sim.getNetInterface(addr).outBW;
		unsigned int level = sim.getNode(addr).getSNLevel();
		sentFraction[maxLevel].addValue((i->bytesSent / totalTime) / outBW);
		maxOut1sFraction[maxLevel].addValue(i->maxBytesOut1sec / outBW);
		maxOut10sFraction[maxLevel].addValue((i->maxBytesOut10sec / 10.0) / outBW);
		recvFraction[maxLevel].addValue((i->bytesReceived / totalTime) / inBW);
		maxIn1sFraction[maxLevel].addValue(i->maxBytesIn1sec / inBW);
		maxIn10sFraction[maxLevel].addValue((i->maxBytesIn10sec / 10.0) / inBW);
		if (level != maxLevel) {
			sentFraction[level].addValue((i->bytesSent / totalTime) / outBW);
			maxOut1sFraction[level].addValue(i->maxBytesOut1sec / outBW);
			maxOut10sFraction[level].addValue((i->maxBytesOut10sec / 10.0) / outBW);
			recvFraction[level].addValue((i->bytesReceived / totalTime) / inBW);
			maxIn1sFraction[level].addValue(i->maxBytesIn1sec / inBW);
			maxIn10sFraction[level].addValue((i->maxBytesIn10sec / 10.0) / inBW);
		}
	}
	os << "# Fraction of total outgoing bandwidth per level" << endl;
	for (unsigned int j = 0; j < maxLevel; j++)
		os << "# Level " << j << endl << CDF(sentFraction[j]) << endl << endl;
	os << "# All nodes" << endl << CDF(sentFraction[maxLevel]) << endl << endl;
	os << "# Fraction of outgoing bandwidth in 1sec interval per level" << endl;
	for (unsigned int j = 0; j < maxLevel; j++)
		os << "# Level " << j << endl << CDF(maxOut1sFraction[j]) << endl << endl;
	os << "# All nodes" << endl << CDF(maxOut1sFraction[maxLevel]) << endl << endl;
	os << "# Fraction of outgoing bandwidth in 10sec interval per level" << endl;
	for (unsigned int j = 0; j < maxLevel; j++)
		os << "# Level " << j << endl << CDF(maxOut10sFraction[j]) << endl << endl;
	os << "# All nodes" << endl << CDF(maxOut1sFraction[maxLevel]) << endl << endl;
	os << "# Fraction of total incoming bandwidth per level" << endl;
	for (unsigned int j = 0; j < maxLevel; j++)
		os << "# Level " << j << endl << CDF(recvFraction[j]) << endl << endl;
	os << "# All nodes" << endl << CDF(recvFraction[maxLevel]) << endl << endl;
	os << "# Fraction of incoming bandwidth in 1sec interval per level" << endl;
	for (unsigned int j = 0; j < maxLevel; j++)
		os << "# Level " << j << endl << CDF(maxIn1sFraction[j]) << endl << endl;
	os << "# All nodes" << endl << CDF(maxIn1sFraction[maxLevel]) << endl << endl;
	os << "# Fraction of incoming bandwidth in 10sec interval per level" << endl;
	for (unsigned int j = 0; j < maxLevel; j++)
		os << "# Level " << j << endl << CDF(maxIn10sFraction[j]) << endl << endl;
	os << "# All nodes" << endl << CDF(maxIn10sFraction[maxLevel]) << endl << endl;

	os << "# Statistics by message type and level" << endl;
	os << "# Level, msg name, sent msgs, sent bytes, min sent size, max sent size, recv msgs, recv bytes, min recv size, max recv size" << endl;
	if (typeSentStatistics.size() < typeRecvStatistics.size()) typeSentStatistics.resize(typeRecvStatistics.size());
	else typeRecvStatistics.resize(typeSentStatistics.size());
	for (unsigned int level = 0; level < typeSentStatistics.size(); level++) {
		for (map<string, MessageType>::iterator it = typeSentStatistics[level].begin(); it != typeSentStatistics[level].end(); it++) {
			MessageType & mt = typeRecvStatistics[level][it->first];
			os << level << ',' << it->first << ',' << it->second.numMessages << ',' << it->second.totalBytes << ',' << it->second.minSize << ','
				<< it->second.maxSize << ',' << mt.numMessages << ',' << mt.totalBytes << ',' << mt.minSize << ',' << mt.maxSize << endl;
		}
	}
	os << endl << endl;

	{
		// Network messages type statistics
		os << "# Statistics by type for network and self messages:" << endl;
		os << "# n/s, type, total msg, fr. of msg, min size, max size, mean size, total bytes, fr. of bytes" << endl;
		double totalMessages = 0, totalBytes = 0;
		for (map<string, MessageType>::const_iterator it = typeNetStatistics.begin(); it != typeNetStatistics.end(); it++) {
			totalMessages += it->second.numMessages;
			totalBytes += it->second.totalBytes;
		}
		for (map<string, MessageType>::const_iterator it = typeNetStatistics.begin(); it != typeNetStatistics.end(); it++)
			os << "n," << it->first << ','
				<< it->second.numMessages << ',' << setprecision(6) << fixed << (it->second.numMessages / totalMessages) << ','
				<< it->second.minSize << ',' << it->second.maxSize << ',' << (it->second.totalBytes / (double)it->second.numMessages) << ','
				<< it->second.totalBytes << ',' << (it->second.totalBytes / totalBytes) << endl;

		// Self messages type statistics
		totalMessages = 0; totalBytes = 0;
		for (map<string, MessageType>::const_iterator it = typeSelfStatistics.begin(); it != typeSelfStatistics.end(); it++) {
			totalMessages += it->second.numMessages;
			totalBytes += it->second.totalBytes;
		}
		for (map<string, MessageType>::const_iterator it = typeSelfStatistics.begin(); it != typeSelfStatistics.end(); it++)
			os << "s," << it->first << ','
				<< it->second.numMessages << ',' << setprecision(6) << fixed << (it->second.numMessages / totalMessages) << ','
				<< it->second.minSize << ',' << it->second.maxSize << ',' << (it->second.totalBytes / (double)it->second.numMessages) << ','
				<< it->second.totalBytes << ',' << (it->second.totalBytes / totalBytes) << endl;
	}
	os << endl << endl;

	{
		// Data traffic mean
		double meanDataSent = 0.0, meanDataRecv = 0.0;
		double meanControlSent = 0.0, meanControlRecv = 0.0;
		double numNodes = nodeStatistics.size();
		for (vector<NodeTraffic>::const_iterator i = nodeStatistics.begin(); i != nodeStatistics.end(); i++) {
			meanDataSent += i->dataBytesSent / numNodes;
			meanDataRecv += i->dataBytesRecv / numNodes;
			meanControlSent += i->bytesSent / numNodes;
			meanControlRecv += i->bytesReceived / numNodes;
		}
		os << "Mean control and data bandwidth (sent/received):" << endl;
		os << "  Control traffic: " << setprecision(15) << meanControlSent << '/' << meanControlRecv << endl;
		os << "  Data traffic: " << setprecision(15) << meanDataSent << '/' << meanDataRecv << endl;
	}
}
