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

#ifndef PEERCOMPNODE_H_
#define PEERCOMPNODE_H_

#include <boost/scoped_ptr.hpp>
#include <log4cpp/Priority.hh>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
namespace iost = boost::iostreams;
#include "CommLayer.hpp"
#include "Logger.hpp"
#include "Properties.hpp"
#include "SimAppDatabase.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "ResourceNode.hpp"
#include "Scheduler.hpp"
#include "Dispatcher.hpp"
#include "MSPDispatcher.hpp"
#include "AvailabilityInformation.hpp"
class Simulator;


/**
 * \brief A node of the STaRS platform.
 *
 * This class creates the main object in the STaRS platform. It creates and registers
 * the ResourceNode and StructureNode objects, and attaches the default Scheduler and
 * Dispatcher objects to them.
 */
class StarsNode : public CommLayer {
public:
    class Configuration {
        int minCPU;
        int maxCPU;
        int stepCPU;
        int minMem;
        int maxMem;
        int stepMem;
        int minDisk;
        int maxDisk;
        int stepDisk;
        int policy;
        std::string inFileName;
        fs::ifstream inFile;
        iost::filtering_streambuf<iost::input> in;
        std::string outFileName;
        fs::ofstream outFile;
        iost::filtering_streambuf<iost::output> out;
        friend class StarsNode;

    public:
        enum {
            IBPolicy = 0,
            MMPolicy = 1,
            DPolicy = 2,
            MSPolicy = 3,
        };

        static Configuration & getInstance() {
            static Configuration instance;
            return instance;
        }

        void setup(const Properties & property);

        int getPolicy() const { return policy; }
    };

    static void libStarsConfigure(const Properties & property);

    StarsNode() {}
    StarsNode(const StarsNode & copy) {}
    StarsNode & operator=(const StarsNode & copy) { return *this; }
    void setup(unsigned int addr);
    void finish();
    void fail();

    void packState(std::streambuf & out);

    void unpackState(std::streambuf & out);

    void receiveMessage(uint32_t src, boost::shared_ptr<BasicMsg> msg);

    void setLocalAddress(const CommAddress & local) { localAddress = local; }

    // setup() must be called before these methods
    StructureNode & getS() const { return static_cast<StructureNode &>(*services[SN]); }
    ResourceNode & getE() const { return static_cast<ResourceNode &>(*services[RN]); }
    SubmissionNode & getSub() const { return static_cast<SubmissionNode &>(*services[Sub]); }
    Scheduler & getScheduler() const { return static_cast<Scheduler &>(*services[Sch]); }
    DispatcherInterface & getDispatcher() const { return static_cast<DispatcherInterface &>(*services[Disp]); }
    SimAppDatabase & getDatabase() { return db; }

    double getAveragePower() const { return power; }
    unsigned long int getAvailableMemory() const { return mem; }
    unsigned long int getAvailableDisk() const { return disk; }

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
    enum {
        SN = 0,
        RN,
        Sub,
        Sch,
        Disp,
    };

    friend class CommLayer;
    void createServices();

    SimAppDatabase db;
    double power;
    unsigned long int mem;
    unsigned long int disk;
};

#endif /*PEERCOMPNODE_H_*/
