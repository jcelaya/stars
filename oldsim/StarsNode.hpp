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

#ifndef PEERCOMPNODE_H_
#define PEERCOMPNODE_H_

#include <boost/scoped_ptr.hpp>
#include <log4cpp/Priority.hh>
#include "CommLayer.hpp"
#include "Logger.hpp"
#include "Properties.hpp"
#include "SimAppDatabase.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "ResourceNode.hpp"
#include "Scheduler.hpp"
#include "Dispatcher.hpp"
#include "MinSlownessDispatcher.hpp"
#include "AvailabilityInformation.hpp"
class Simulator;


/**
 * \brief A node of the PeerComp platform.
 *
 * This class creates the main object in the PeerComp platform. It creates and registers
 * the ResourceNode and StructureNode objects, and attaches the default Scheduler and
 * Dispatcher objects to them.
 */
class StarsNode : public CommLayer {
public:

    enum {
        SimpleSchedulerClass = 0,
        FCFSSchedulerClass = 1,
        EDFSchedulerClass = 2,
        MSSchedulerClass = 3,
    };

    static void libStarsConfigure(const Properties & property);

    StarsNode() {}
    StarsNode(const StarsNode & copy) {}
    StarsNode & operator=(const StarsNode & copy) { return *this; }
    void setup(unsigned int addr);
    void finish();

    void packState(std::streambuf & out);

    void unpackState(std::streambuf & out);

    void receiveMessage(uint32_t src, boost::shared_ptr<BasicMsg> msg) {
        enqueueMessage(CommAddress(src, ConfigurationManager::getInstance().getPort()), msg);
        processNextMessage();
    }

    void setLocalAddress(const CommAddress & local) { localAddress = local; }

    // setup() must be called before these methods
    StructureNode & getS() const { return *structureNode; }
    ResourceNode & getE() const { return *resourceNode; }
    SubmissionNode & getSub() const { return *submissionNode; }
    Scheduler & getScheduler() const { return *scheduler; }
    SimAppDatabase & getDatabase() { return db; }

    double getAveragePower() const { return power; }
    unsigned long int getAvailableMemory() const { return mem; }
    unsigned long int getAvailableDisk() const { return disk; }
    int getSchedulerType() const { return schedulerType; }

    boost::shared_ptr<AvailabilityInformation> getBranchInfo() const;
    boost::shared_ptr<AvailabilityInformation> getChildInfo(const CommAddress & child) const;
    unsigned int getSNLevel() const;

    void showRecursive(log4cpp::Priority::Value prio, unsigned int level, const std::string & prefix = "");
    void showPartialTree(bool isBranch, log4cpp::Priority::Value prio = log4cpp::Priority::DEBUG);
    unsigned int getRoot() const;
    static void showTree(log4cpp::Priority::Value p = log4cpp::Priority::DEBUG);
    static void checkTree();

    void generateRNode(uint32_t rfather);
    void generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, int level);
    void generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level);
    template <class T>
    void generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, int level);
    template <class T>
    void generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level);

    friend std::ostream & operator<<(std::ostream & os, const StarsNode & n) {
        return os << n.power << " MIPS " << n.mem << " MB " << n.disk << " MB";
    }

private:
    void createServices();

    int schedulerType;
    boost::scoped_ptr<StructureNode> structureNode;
    boost::scoped_ptr<ResourceNode> resourceNode;
    boost::scoped_ptr<SubmissionNode> submissionNode;
    boost::scoped_ptr<Scheduler> scheduler;
    boost::scoped_ptr<DispatcherInterface> dispatcher;
    SimAppDatabase db;
    double power;
    unsigned long int mem;
    unsigned long int disk;
};

#endif /*PEERCOMPNODE_H_*/