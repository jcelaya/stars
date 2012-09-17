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

#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/scoped_ptr.hpp>
namespace iost = boost::iostreams;
#include <msg/msg.h>
#include "CommLayer.hpp"
#include "Logger.hpp"
#include "Properties.hpp"
#include "SimAppDatabase.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "ResourceNode.hpp"
#include "Scheduler.hpp"
#include "Dispatcher.hpp"
#include "AvailabilityInformation.hpp"


/**
 * \brief A node of the STaRS platform.
 *
 * This class creates the main object in the STaRS platform. It creates and registers
 * the ResourceNode and StructureNode objects, and attaches the default Scheduler and
 * Dispatcher objects to them.
 */
class StarsNode : public CommLayer {
public:
    enum {
        SimpleSchedulerClass = 0,
        FCFSSchedulerClass = 1,
        EDFSchedulerClass = 2,
        MinSlownessSchedulerClass = 3,
    };

    enum {
        SN = 0,
        RN,
        Sub,
        Sch,
        Disp,
    };

    StarsNode() {}
    StarsNode(const StarsNode & copy) {}

    void setup(unsigned int addr, m_host_t host);

    void finish();

    void packState(std::streambuf & out);

    void unpackState(std::streambuf & out);

    m_host_t getHost() const {
        return simHost;
    }
    const std::string & getMailbox() const {
        return mailbox;
    }

    // createServices() must be called before these methods
    StructureNode & getStructureNode() const { return static_cast<StructureNode &>(*services[SN]); }
    ResourceNode & getResourceNode() const { return static_cast<ResourceNode &>(*services[RN]); }
    SubmissionNode & getSubmissionNode() const { return static_cast<SubmissionNode &>(*services[Sub]); }
    Scheduler & getScheduler() const { return static_cast<Scheduler &>(*services[Sch]); }
    DispatcherInterface & getDispatcher() const { return static_cast<DispatcherInterface &>(*services[Disp]); }
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

    //     boost::shared_ptr<AvailabilityInformation> getChildInfo(const CommAddress & child) const;
    //     unsigned int getSNLevel() const;
    //
    //     void showRecursive(log4cpp::Priority::Value prio, unsigned int level, const std::string & prefix = "");
    //     void showPartialTree(bool isBranch, log4cpp::Priority::Value prio = log4cpp::Priority::DEBUG);
    //     unsigned int getRoot() const;
    //     static void showTree(log4cpp::Priority::Value p = log4cpp::Priority::DEBUG);
    //     static void checkTree();

    friend std::ostream & operator<<(std::ostream & os, const StarsNode & n) {
        return os << n.power << " MIPS " << n.mem << " MB " << n.disk << " MB";
    }

    /**
     * The entry point for every process. Each host is assigned this function as its entry
     * point. The private data of each process is its corresponding StarsNode object.
     */
    static int processFunction(int argc, char * argv[]);

    /**
     * Measures the size of a serialized BasicMsg-derived object.
     */
    static unsigned long int getMsgSize(BasicMsg * msg);

    static void libStarsConfigure(const Properties & property);

private:
    int mainLoop();

    void createServices();
    void destroyServices();

    m_host_t simHost;
    std::string mailbox;
    int schedulerType;
    SimAppDatabase db;
    double power;
    unsigned long int mem;
    unsigned long int disk;
};

#endif /*STARSNODE_H_*/
