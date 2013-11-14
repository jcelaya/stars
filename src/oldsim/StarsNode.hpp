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

#ifndef STARSNODE_H_
#define STARSNODE_H_

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
#include "SimOverlayBranch.hpp"
#include "SimOverlayLeaf.hpp"
#include "SubmissionNode.hpp"
#include "Scheduler.hpp"
#include "Dispatcher.hpp"
#include "AvailabilityInformation.hpp"
#include "Variables.hpp"
#include "PolicyFactory.hpp"
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
        Configuration();

        DiscreteParetoVariable cpuVar;
        DiscreteUniformVariable memVar, diskVar;
        std::unique_ptr<PolicyFactory> policy;
        std::string inFileName;
        fs::ifstream inFile;
        iost::filtering_streambuf<iost::input> in;
        std::string outFileName;
        fs::ofstream outFile;
        iost::filtering_streambuf<iost::output> out;
        friend class StarsNode;

    public:
        static Configuration & getInstance() {
            static Configuration instance;
            return instance;
        }

        void setup(const Properties & property);

        const std::unique_ptr<PolicyFactory> & getPolicy() const { return policy; }
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
    OverlayBranch & getBranch() const { return static_cast<OverlayBranch &>(*services[Branch]); }
    OverlayLeaf & getLeaf() const { return static_cast<OverlayLeaf &>(*services[Leaf]); }
    SubmissionNode & getSub() const { return static_cast<SubmissionNode &>(*services[Sub]); }
    Scheduler & getSch() const { return static_cast<Scheduler &>(*services[Sch]); }
    DispatcherInterface & getDisp() const { return static_cast<DispatcherInterface &>(*services[Disp]); }
    SimAppDatabase & getDatabase() { return db; }

    double getAveragePower() const { return power; }
    unsigned long int getAvailableMemory() const { return mem; }
    unsigned long int getAvailableDisk() const { return disk; }

    unsigned int getBranchLevel() const;

    void buildDispatcher();
    void buildDispatcherDown();

    friend std::ostream & operator<<(std::ostream & os, const StarsNode & n) {
        return os << n.power << " MIPS " << n.mem << " MB " << n.disk << " MB";
    }

private:
    enum {
        Branch = 0,
        Leaf,
        Sub,
        Sch,
        Disp,
    };

    friend class CommLayer;
    void createServices();
    template <class T> void buildDispatcherGen();
    template <class T> void buildDispatcherDownGen();

    SimAppDatabase db;
    double power;
    unsigned long int mem;
    unsigned long int disk;
};

#endif /*STARSNODE_H_*/
