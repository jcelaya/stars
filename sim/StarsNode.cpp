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

#include <sstream>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include "StarsNode.hpp"
#include "ResourceNode.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "EDFScheduler.hpp"
#include "DeadlineDispatcher.hpp"
#include "FCFSScheduler.hpp"
#include "QueueBalancingDispatcher.hpp"
#include "SimpleScheduler.hpp"
#include "SimpleDispatcher.hpp"
#include "MinSlownessScheduler.hpp"
#include "MinSlownessDispatcher.hpp"
#include "Simulator.hpp"
#include "Time.hpp"
#include "SimTask.hpp"
#include "Logger.hpp"
#include "ConfigurationManager.hpp"
using namespace std;
using namespace boost::iostreams;
using namespace boost::posix_time;
using namespace boost::gregorian;
using boost::shared_ptr;
using boost::scoped_ptr;


class SimExecutionEnvironment : public Scheduler::ExecutionEnvironment {
public:
    SimExecutionEnvironment() {}

    double getAveragePower() const {
        return Simulator::getInstance().getCurrentNode().getAveragePower();
    }

    unsigned long int getAvailableMemory() const {
        return Simulator::getInstance().getCurrentNode().getAvailableMemory();
    }

    unsigned long int getAvailableDisk() const {
        return Simulator::getInstance().getCurrentNode().getAvailableDisk();
    }

    shared_ptr<Task> createTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d) const {
        return shared_ptr<Task>(new SimTask(o, reqId, ctid, d));
    }
};


// Rewritten methods
// Execution environment
Scheduler::ExecutionEnvironmentImpl::ExecutionEnvironmentImpl()
        : impl(new SimExecutionEnvironment()) {}


// CommLayer
CommLayer::CommLayer() : exitSignaled(false) {}


// The following functions must allways be called from an agent's thread
CommLayer & CommLayer::getInstance() {
    return Simulator::getInstance().getCurrentNode();
}


namespace {
    void deleteFailedMsg(void * task) {
        delete static_cast<BasicMsg *>(MSG_task_get_data(static_cast<m_task_t>(task)));
        MSG_task_destroy(static_cast<m_task_t>(task));
    }
}


unsigned int CommLayer::sendMessage(const CommAddress & dst, BasicMsg * msg) {
    if (dst == getLocalAddress()) {
        enqueueMessage(dst, boost::shared_ptr<BasicMsg>(msg));
        return 0;
    } else {
        unsigned long int size = StarsNode::getMsgSize(msg) + 90;
        MSG_task_dsend(MSG_task_create("m", 0, size, msg), Simulator::getInstance().getNode(dst.getIPNum()).getMailbox().c_str(), deleteFailedMsg);
        return size;
    }
}


// Time
Time Time::getCurrentTime() {
    return Time((int64_t)(MSG_get_clock() * 1000000));
}


int CommLayer::setTimerImpl(Time time, shared_ptr<BasicMsg> msg) {
    // Add a task to the timer structure
    Timer t(time, msg);
    timerList.push_front(t);
    timerList.sort();
    return t.id;
}


std::ostream & operator<<(std::ostream & os, const Time & r) {
    return os << Duration(r.getRawDate());
}


// Configuration object
struct StarsNodeConfiguration {
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
    std::string outFileName;
    fs::ofstream outFile;
    iost::filtering_streambuf<iost::output> out;
    
    static StarsNodeConfiguration & getInstance() {
        static StarsNodeConfiguration instance;
        return instance;
    }
    
    void setup(const Properties & property) {
        minMem = property("min_mem", 256);
        maxMem = property("max_mem", 4096);
        stepMem = property("step_mem", 256);
        minDisk = property("min_disk", 64);
        maxDisk = property("max_disk", 1000);
        stepDisk = property("step_disk", 100);
        inFileName = property("in_file", string(""));
        outFileName = property("out_file", string(""));
        string s = property("scheduler", string(""));
        if (s == "DS") sched = StarsNode::EDFSchedulerClass;
        else if (s == "MS") sched = StarsNode::MinSlownessSchedulerClass;
        else if (s == "FCFS") sched = StarsNode::FCFSSchedulerClass;
        else sched = StarsNode::SimpleSchedulerClass;
        if (inFileName != "") {
            inFile.exceptions(ios_base::failbit | ios_base::badbit);
            inFile.open(inFileName, ios_base::binary);
            if (inFileName.substr(inFileName.length() - 3) == ".gz")
                in.push(iost::gzip_decompressor());
            in.push(inFile);
        }
        if (outFileName != "") {
            outFile.exceptions(ios_base::failbit | ios_base::badbit);
            outFile.open(outFileName, ios_base::binary);
            if (outFileName.substr(outFileName.length() - 3) == ".gz")
                out.push(iost::gzip_compressor());
            out.push(outFile);
        }
    }
};


void StarsNode::libStarsConfigure(const Properties & property) {
    ConfigurationManager::getInstance().setUpdateBandwidth(property("update_bw", 1000.0));
    ConfigurationManager::getInstance().setSlownessRatio(property("stretch_ratio", 2.0));
    ConfigurationManager::getInstance().setHeartbeat(property("heartbeat", 300));
    ConfigurationManager::getInstance().setWorkingPath(Simulator::getInstance().getResultDir());
    unsigned int clustersBase = property("avail_clusters_base", 3U);
    if (clustersBase) {
        BasicAvailabilityInfo::setNumClusters(clustersBase * clustersBase);
        QueueBalancingInfo::setNumClusters(clustersBase * clustersBase * clustersBase * clustersBase);
        TimeConstraintInfo::setNumClusters(clustersBase * clustersBase * clustersBase);
    }
    TimeConstraintInfo::setNumRefPoints(property("tci_ref_points", 8U));
    StarsNodeConfiguration::getInstance().setup(property);
}


void StarsNode::setup(unsigned int addr, m_host_t host) {
    localAddress = CommAddress(addr, ConfigurationManager::getInstance().getPort());
    simHost = host;
    ostringstream oss;
    oss << localAddress;
    mailbox = oss.str();
    StarsNodeConfiguration & cfg = StarsNodeConfiguration::getInstance();
    power = MSG_get_host_speed(simHost);
    // NOTE: using the same seed generates the same set of memory and disk values between simulations
//     const char * memoryStr = MSG_host_get_property_value(simHost, "mem");
//     if (memoryStr)
//         istringstream(string(memoryStr)) >> mem;
//     else
        mem = Simulator::uniform(cfg.minMem, cfg.maxMem, cfg.stepMem);
//     const char * diskStr = MSG_host_get_property_value(simHost, "dsk");
//     if (diskStr)
//         istringstream(string(diskStr)) >> disk;
//     else
        disk = Simulator::uniform(cfg.minDisk, cfg.maxDisk, cfg.stepDisk);
    schedulerType = cfg.sched;

    createServices();
    // Load service state if needed
    if (cfg.inFile.is_open()) {
        unpackState(cfg.in);
    }
}


int StarsNode::processFunction(int argc, char * argv[]) {
    return Simulator::getInstance().getCurrentNode().mainLoop();
}


int StarsNode::mainLoop() {
    Simulator & sim = Simulator::getInstance();
    LogMsg("Sim.Process", INFO) << "Peer running at " << MSG_host_get_name(simHost) << " with address " << localAddress;
    
    // Message loop
    while(sim.doContinue()) {
        // Event loop: receive messages from the simulator and treat them
        m_task_t task = NULL;
        double timeout = timerList.empty() ? 5.0 : (timerList.front().timeout - Time::getCurrentTime()).seconds();
        msg_comm_t comm = MSG_task_irecv(&task, mailbox.c_str());
        if (MSG_comm_wait(comm, timeout) == MSG_OK) {
            MSG_comm_destroy(comm);
            BasicMsg * msg = static_cast<BasicMsg *>(MSG_task_get_data(task));
            const CommAddress & src = static_cast<StarsNode *>(MSG_host_get_data(MSG_task_get_source(task)))->getLocalAddress();
            enqueueMessage(src, boost::shared_ptr<BasicMsg>(msg));
        } else {
            MSG_comm_destroy(comm);
            // Check timers
            Time ct = Time::getCurrentTime();
            while (!timerList.empty() && timerList.front().timeout <= ct) {
                enqueueMessage(localAddress, timerList.front().msg);
                timerList.pop_front();
            }
        }
        while (!messageQueue.empty()) {
            boost::shared_ptr<BasicMsg> msg = messageQueue.front().second;
            CommAddress dst = messageQueue.front().first;
            sim.getCase().beforeEvent(localAddress, dst, msg);
            sim.getPerformanceStatistics().startEvent(msg->getName());
            processNextMessage();
            sim.getPerformanceStatistics().endEvent(msg->getName());
            sim.getCase().afterEvent(localAddress, dst, msg);
        }
    }
    
    // Cleanup
    services.clear();

    return 0;
}


void StarsNode::finish()  {
    StarsNodeConfiguration & cfg = StarsNodeConfiguration::getInstance();
    if (cfg.outFile.is_open()) {
        packState(cfg.out);
    }
    destroyServices();
}


class MsgpackOutArchive {
    msgpack::packer<msgpack::sbuffer> & pk;
    static const bool valid, invalid;
public:
    MsgpackOutArchive(msgpack::packer<msgpack::sbuffer> & o) : pk(o) {}
    template<class T> MsgpackOutArchive & operator&(T & o) {
        pk.pack(o);
        return *this;
    }
    template<class T> MsgpackOutArchive & operator&(boost::shared_ptr<T> & o) {
        if (o.get()) {
            pk.pack(valid);
            *this & *o;
        } else {
            pk.pack(invalid);
        }
        return *this;
    }
    template<class T> MsgpackOutArchive & operator&(std::list<T> & o) {
        size_t size = o.size();
        pk.pack(size);
        for (typename std::list<T>::iterator i = o.begin(); i != o.end(); ++i)
            *this & *i;
        return *this;
    }
};

template<> inline MsgpackOutArchive & MsgpackOutArchive::operator&(TransactionalZoneDescription & o) {
    o.serializeState(*this);
    return *this;
}

const bool MsgpackOutArchive::valid = true, MsgpackOutArchive::invalid = false;


void StarsNode::packState(std::streambuf & out) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);
    MsgpackOutArchive ar(pk);
    structureNode->serializeState(ar);
    resourceNode->serializeState(ar);
    switch (schedulerType) {
        case SimpleSchedulerClass:
            static_cast<SimpleDispatcher &>(*dispatcher).serializeState(ar);
            break;
        case FCFSSchedulerClass:
            static_cast<QueueBalancingDispatcher &>(*dispatcher).serializeState(ar);
            break;
        case EDFSchedulerClass:
            static_cast<DeadlineDispatcher &>(*dispatcher).serializeState(ar);
            break;
        case MinSlownessSchedulerClass:
            static_cast<MinSlownessDispatcher &>(*dispatcher).serializeState(ar);
            break;
        default:
            break;
    }
    uint32_t size = buffer.size();
    out.sputn((const char *)&size, 4);
    out.sputn(buffer.data(), size);
}


class MsgpackInArchive {
    msgpack::unpacker & upk;
    msgpack::unpacked msg;
public:
    MsgpackInArchive(msgpack::unpacker & o) : upk(o) {}
    template<class T> MsgpackInArchive & operator&(T & o) {
        upk.next(&msg); msg.get().convert(&o);
        return *this;
    }
    template<class T> MsgpackInArchive & operator&(boost::shared_ptr<T> & o) {
        bool valid;
        upk.next(&msg); msg.get().convert(&valid);
        if (valid) {
            o.reset(new T());
            *this & *o;
        } else o.reset();
        return *this;
    }
    template<class T> MsgpackInArchive & operator&(std::list<T> & o) {
        size_t size;
        upk.next(&msg); msg.get().convert(&size);
        for (size_t i = 0; i < size; ++i) {
            o.push_back(T());
            *this & o.back();
        }
        return *this;
    }
};

template<> inline MsgpackInArchive & MsgpackInArchive::operator&(TransactionalZoneDescription & o) {
    o.serializeState(*this);
    return *this;
}


void StarsNode::unpackState(std::streambuf & in) {
    uint32_t size;
    in.sgetn((char *)&size, 4);
    msgpack::unpacker upk;
    upk.reserve_buffer(size);
    in.sgetn(upk.buffer(), size);
    upk.buffer_consumed(size);
    MsgpackInArchive ar(upk);
    structureNode->serializeState(ar);
    resourceNode->serializeState(ar);
    switch (schedulerType) {
        case SimpleSchedulerClass:
            static_cast<SimpleDispatcher &>(*dispatcher).serializeState(ar);
            break;
        case FCFSSchedulerClass:
            static_cast<QueueBalancingDispatcher &>(*dispatcher).serializeState(ar);
            break;
        case EDFSchedulerClass:
            static_cast<DeadlineDispatcher &>(*dispatcher).serializeState(ar);
            break;
        case MinSlownessSchedulerClass:
            static_cast<MinSlownessDispatcher &>(*dispatcher).serializeState(ar);
            break;
        default:
            break;
    }
}


void StarsNode::createServices() {
    structureNode.reset(new StructureNode(2));
    resourceNode.reset(new ResourceNode(*structureNode));
    submissionNode.reset(new SubmissionNode(*resourceNode));
    switch (schedulerType) {
        case StarsNode::FCFSSchedulerClass:
            scheduler.reset(new FCFSScheduler(*resourceNode));
            dispatcher.reset(new QueueBalancingDispatcher(*structureNode));
            break;
        case StarsNode::EDFSchedulerClass:
            scheduler.reset(new EDFScheduler(*resourceNode));
            dispatcher.reset(new DeadlineDispatcher(*structureNode));
            break;
        case StarsNode::MinSlownessSchedulerClass:
            scheduler.reset(new MinSlownessScheduler(*resourceNode));
            dispatcher.reset(new MinSlownessDispatcher(*structureNode));
            break;
        default:
            scheduler.reset(new SimpleScheduler(*resourceNode));
            dispatcher.reset(new SimpleDispatcher(*structureNode));
            break;
    }
}


void StarsNode::destroyServices() {
    // Gracely terminate the services
    dispatcher.reset();
    scheduler.reset();
    submissionNode.reset();
    resourceNode.reset();
    structureNode.reset();
}


unsigned long int StarsNode::getMsgSize(BasicMsg * msg) {
    ostringstream oss;
    msgpack::packer<std::ostream> pk(&oss);
    msg->pack(pk);
    return oss.tellp();
}


// unsigned int StarsNode::getSNLevel() const {
//     if (!structureNode->inNetwork())
//         return Simulator::getInstance().getNode(resourceNode->getFather().getIPNum()).getSNLevel() + 1;
//     else if (structureNode->getFather() != CommAddress())
//         return Simulator::getInstance().getNode(structureNode->getFather().getIPNum()).getSNLevel() + 1;
//     else return 0;
// }
// 
// 
// void StarsNode::showRecursive(log4cpp::Priority::Value prio, unsigned int level, const string & prefix) {
//     shared_ptr<AvailabilityInformation> info = getBranchInfo();
//     if (info.get())
//         treeCat << prio << prefix << "S@" << localAddress << ": " << *structureNode << ' ' << *info;
//     else
//         treeCat << prio << prefix << "S@" << localAddress << ": " << *structureNode << " ?";
//     if (level) {
//         for (unsigned int i = 0; i < structureNode->getNumChildren(); i++) {
//             bool last = i == structureNode->getNumChildren() - 1;
//             stringstream father, child;
//             father << prefix << "  " << ((last) ? '\\' : '|') << "- ";
//             child << prefix << "  " << ((last) ? ' ' : '|') << "  ";
// 
//             uint32_t childAddr;
//             if (structureNode->getSubZone(i)->getLink() != CommAddress())
//                 childAddr = structureNode->getSubZone(i)->getLink().getIPNum();
//             else childAddr = structureNode->getSubZone(i)->getNewLink().getIPNum();
//             StarsNode & childNode = Simulator::getInstance().getNode(childAddr);
//             info = getChildInfo(CommAddress(childAddr, ConfigurationManager::getInstance().getPort()));
//             if (info.get())
//                 treeCat << prio << father.str() << *structureNode->getSubZone(i) << ' ' << *info;
//             else
//                 treeCat << prio << father.str() << *structureNode->getSubZone(i) << " ?";
//             if (!structureNode->isRNChildren()) {
//                 childNode.showRecursive(prio, level - 1, child.str());
//             } else {
//                 treeCat << prio << child.str() << "R@" << CommAddress(childAddr, ConfigurationManager::getInstance().getPort()) << ": " << *childNode.resourceNode << " "
//                 << childNode << ' ' << childNode.getScheduler().getAvailability();
//             }
//         }
//     }
// }
// 
// 
// void StarsNode::showPartialTree(bool isBranch, log4cpp::Priority::Value prio) {
//     if (treeCat.isPriorityEnabled(prio)) {
//         StructureNode * father = NULL;
//         uint32_t fatherIP = 0;
//         if (isBranch) {
//             if (structureNode->getFather() != CommAddress()) {
//                 fatherIP = structureNode->getFather().getIPNum();
//                 father = Simulator::getInstance().getNode(fatherIP).structureNode.get();
//             }
//         } else {
//             if (resourceNode->getFather() != CommAddress()) {
//                 fatherIP = resourceNode->getFather().getIPNum();
//                 father = Simulator::getInstance().getNode(fatherIP).structureNode.get();
//             } else {
//                 // This may be an error...
//                 treeCat.warnStream() << "Resource node without father???: R@" << localAddress << ": " << *resourceNode;
//                 return;
//             }
//         }
// 
//         if (father != NULL) {
//             treeCat << prio << "S@" << Simulator::getInstance().getNode(fatherIP).getLocalAddress() << ": " << *father;
//             for (unsigned int i = 0; i < father->getNumChildren(); i++) {
//                 bool last = i == father->getNumChildren() - 1;
//                 treeCat << prio << (last ? "  \\- " : "  |- ") << *father->getSubZone(i);
//                 uint32_t childAddr;
//                 if (father->getSubZone(i)->getLink() != CommAddress())
//                     childAddr = father->getSubZone(i)->getLink().getIPNum();
//                 else childAddr = father->getSubZone(i)->getNewLink().getIPNum();
//                 StarsNode & childNode = Simulator::getInstance().getNode(childAddr);
//                 if (!father->isRNChildren()) {
//                     childNode.showRecursive(prio, childAddr == localAddress.getIPNum() ? 1 : 0, (last ? "     " : "  |  "));
//                 } else {
//                     treeCat << prio << (last ? "     " : "  |  ") << "R@" << childNode.localAddress << ": " << *childNode.resourceNode;
//                 }
//             }
//         } else showRecursive(prio, 1);
//     }
// }
// 
// 
// unsigned int StarsNode::getRoot() const {
//     const CommAddress & father = structureNode->inNetwork() ? structureNode->getFather() : resourceNode->getFather();
//     if (father != CommAddress()) return Simulator::getInstance().getNode(father.getIPNum()).getRoot();
//     else return localAddress.getIPNum();
// }
// 
// 
// void StarsNode::showTree(log4cpp::Priority::Value prio) {
//     if (treeCat.isPriorityEnabled(prio)) {
//         treeCat << prio << "Final tree:";
//         for (unsigned int i = 0; i < Simulator::getInstance().getNumNodes(); i++) {
//             unsigned int rootIP = Simulator::getInstance().getNode(i).getRoot();
//             StarsNode & root = Simulator::getInstance().getNode(rootIP);
//             if (root.structureNode->inNetwork()) {
//                 root.showRecursive(prio, -1);
//                 treeCat << prio << "";
//                 treeCat << prio << "";
//                 break;
//             }
//         }
//     }
// }
// 
// 
// void StarsNode::checkTree() {
//     unsigned int root0 = Simulator::getInstance().getNode(0).getRoot();
//     for (unsigned int i = 1; i < Simulator::getInstance().getNumNodes(); i++) {
//         unsigned int root = Simulator::getInstance().getNode(i).getRoot();
//         if (root != root0) {
//             treeCat.errorStream() << "Node " << CommAddress(i, ConfigurationManager::getInstance().getPort()) << " outside main tree";
//         }
//     }
// }
