/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
 *
 *  This file is part of STaRS.
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

#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/scoped_ptr.hpp>
namespace iost = boost::iostreams;
#include <msg/msg.h>
#include "CommLayer.hpp"
#include "Logger.hpp"
#include "Properties.hpp"
#include "SimAppDatabase.hpp"
#include "portable_binary_iarchive.hpp"
#include "portable_binary_oarchive.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "ResourceNode.hpp"
#include "Scheduler.hpp"
#include "Dispatcher.hpp"
#include "MinStretchDispatcher.hpp"
#include "AvailabilityInformation.hpp"
class Simulator;


class PeerCompNodeFactory;

/**
 * \brief A node of the PeerComp platform.
 *
 * This class creates the main object in the PeerComp platform. It creates and registers
 * the ResourceNode and StructureNode objects, and attaches the default Scheduler and
 * Dispatcher objects to them.
 */
class PeerCompNode : public CommLayer {
public:
    enum {
        SimpleSchedulerClass = 0,
        FCFSSchedulerClass = 1,
        EDFSchedulerClass = 2,
        MinStretchSchedulerClass = 3,
    };
    
    PeerCompNode() {}
    PeerCompNode(const PeerCompNode & copy) {}
    
    void setAddressAndHost(unsigned int addr, m_host_t host);
    
    m_host_t getHost() const {
        return simHost;
    }
    const std::string & getMailbox() const {
        return mailbox;
    }

    // setup() must be called before these methods
    StructureNode & getS() const {
        return *structureNode;
    }
    ResourceNode & getE() const {
        return *resourceNode;
    }
    SubmissionNode & getSub() const {
        return *submissionNode;
    }
    Scheduler & getScheduler() const {
        return *scheduler;
    }
    SimAppDatabase & getDatabase() {
        return db;
    }
    
    double getAveragePower() const {
        return power;
    }
    unsigned long int getAvailableMemory() const {
        return mem;
    }
    unsigned long int getAvailableDisk() const {
        return disk;
    }
    int getSchedulerType() const {
        return schedulerType;
    }
    
    //     boost::shared_ptr<AvailabilityInformation> getBranchInfo() const;
    //     boost::shared_ptr<AvailabilityInformation> getChildInfo(const CommAddress & child) const;
    //     unsigned int getSNLevel() const;
    // 
    //     void showRecursive(log4cpp::Priority::Value prio, unsigned int level, const std::string & prefix = "");
    //     void showPartialTree(bool isBranch, log4cpp::Priority::Value prio = log4cpp::Priority::DEBUG);
    //     unsigned int getRoot() const;
    //     static void showTree(log4cpp::Priority::Value p = log4cpp::Priority::DEBUG);
    //     static void checkTree();
    //     static void saveState(const Properties & property);
    // 
    //     void serializeState(portable_binary_oarchive & ar);
    //     void serializeState(portable_binary_iarchive & ar);
    //     void generateRNode(uint32_t rfather);
    //     void generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, int level);
    //     void generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level);
    //     template <class T>
    //     void generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, int level);
    //     template <class T>
    //     void generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level);
    
    friend std::ostream & operator<<(std::ostream & os, const PeerCompNode & n) {
        return os << n.power << " MIPS " << n.mem << " MB " << n.disk << " MB";
    }
    
    /**
     * The entry point for every process. Each host is assigned this function as its entry
     * point. The private data of each process is its corresponding PeerCompNode object.
     */
    static int processFunction(int argc, char * argv[]);
    
    /**
     * Measures the size of a serialized BasicMsg-derived object.
     */
    static unsigned long int getMsgSize(BasicMsg * msg);
    
private:
    void mainLoop();
    
    void createServices();
    void destroyServices();
    
    friend class PeerCompNodeFactory;

    m_host_t simHost;
    std::string mailbox;
    int schedulerType;
    boost::scoped_ptr<StructureNode> structureNode;
    boost::scoped_ptr<ResourceNode> resourceNode;
    boost::scoped_ptr<SubmissionNode> submissionNode;
    boost::scoped_ptr<Scheduler> scheduler;
    boost::scoped_ptr<DispatcherInterface> dispatcher;
    boost::scoped_ptr<MinStretchDispatcher> minStretchDisp;
    SimAppDatabase db;
    double power;
    unsigned long int mem;
    unsigned long int disk;
};


class PeerCompNodeFactory {
    int fanout;
    double minCPU;
    double maxCPU;
    double stepCPU;
    int minMem;
    int maxMem;
    int stepMem;
    int minDisk;
    int maxDisk;
    int stepDisk;
    int sched;
    std::string inFileName;
    fs::ifstream inFile;
    iost::filtering_streambuf<iost::input> in;
    boost::scoped_ptr<portable_binary_iarchive> ia;
    
    PeerCompNodeFactory() {}

public:
    static PeerCompNodeFactory & getInstance() {
        static PeerCompNodeFactory instance;
        return instance;
    }
    
    void setupFactory(const Properties & property);

    void setupNode(PeerCompNode & node);
};

#endif /*PEERCOMPNODE_H_*/
