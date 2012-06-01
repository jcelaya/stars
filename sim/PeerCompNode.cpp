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
#include "PeerCompNode.hpp"
#include "ResourceNode.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "EDFScheduler.hpp"
#include "DeadlineDispatcher.hpp"
#include "FCFSScheduler.hpp"
#include "QueueBalancingDispatcher.hpp"
#include "SimpleScheduler.hpp"
#include "SimpleDispatcher.hpp"
#include "MinStretchScheduler.hpp"
#include "MinStretchDispatcher.hpp"
#include "Simulator.hpp"
#include "Time.hpp"
#include "SimTask.hpp"
#include "Logger.hpp"
using namespace std;
using namespace boost::iostreams;
using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::archive;
using boost::shared_ptr;
using boost::scoped_ptr;


class SimExecutionEnvironment : public Scheduler::ExecutionEnvironment {
    PeerCompNode & node;
public:
    SimExecutionEnvironment() : node(Simulator::getCurrentNode()) {}

    double getAveragePower() const {
        return node.getAveragePower();
    }

    unsigned long int getAvailableMemory() const {
        return node.getAvailableMemory();
    }

    unsigned long int getAvailableDisk() const {
        return node.getAvailableDisk();
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
    return Simulator::getCurrentNode();
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
        unsigned long int size = PeerCompNode::getMsgSize(msg) + 90;
        MSG_task_dsend(MSG_task_create("foo", 0, size, msg), Simulator::getInstance().getNode(dst.getIPNum()).getMailbox().c_str(), deleteFailedMsg);
        return size;
    }
}


int CommLayer::setTimerImpl(Time time, shared_ptr<BasicMsg> msg) {
    // Add a task to the timer structure
    Timer t(time, msg);
    timerList.push_front(t);
    timerList.sort();
    return t.id;
}


// Time
Time Time::getCurrentTime() {
    return Time(int64_t(MSG_get_clock() * 1000000));
}


std::ostream & operator<<(std::ostream & os, const Time & r) {
    return os << Duration(r.getRawDate());
}


// Factory methods
void PeerCompNodeFactory::setupFactory(const Properties & property) {
    SimAppDatabase::reset();
    fanout = property("fanout", 2);
    minCPU = property("min_cpu", 1000.0);
    maxCPU = property("max_cpu", 3000.0);
    stepCPU = property("step_cpu", 200.0);
    minMem = property("min_mem", 256);
    maxMem = property("max_mem", 4096);
    stepMem = property("step_mem", 256);
    minDisk = property("min_disk", 64);
    maxDisk = property("max_disk", 1000);
    stepDisk = property("step_disk", 100);
    inFileName = property("in_file", string(""));
    unsigned int clustersBase = property("avail_clusters_base", 0U);
    if (clustersBase) {
        BasicAvailabilityInfo::setNumClusters(clustersBase * clustersBase);
        QueueBalancingInfo::setNumClusters(clustersBase * clustersBase * clustersBase * clustersBase);
        TimeConstraintInfo::setNumClusters(clustersBase * clustersBase * clustersBase);
    }
    TimeConstraintInfo::setNumRefPoints(property("tci_ref_points", 8U));
    if (inFileName == "") {
        string s = property("scheduler", string("DS"));
        if (s == "DS") sched = PeerCompNode::EDFSchedulerClass;
        else if (s == "MS") sched = PeerCompNode::MinStretchSchedulerClass;
        else if (s == "FCFS") sched = PeerCompNode::FCFSSchedulerClass;
        else if (s == "SS") sched = PeerCompNode::SimpleSchedulerClass;
        else sched = PeerCompNode::EDFSchedulerClass;
    } else {
        sched = -1;
        inFile.exceptions(ios_base::failbit | ios_base::badbit);
        inFile.open(inFileName, ios_base::binary);
        in.push(iost::zlib_decompressor());
        in.push(inFile);
        ia.reset(new portable_binary_iarchive(in, 0));
    }
}


void PeerCompNodeFactory::setupNode(PeerCompNode & node) {
    // Execution power follows discretized pareto distribution, with k=1
    node.power = Simulator::discretePareto(minCPU, maxCPU, stepCPU, 1.0);
    node.mem = Simulator::uniform(minMem, maxMem, stepMem);
    node.disk = Simulator::uniform(minDisk, maxDisk, stepDisk);
    node.structureNode.reset(new StructureNode(fanout));
    node.resourceNode.reset(new ResourceNode(*node.structureNode));
    node.submissionNode.reset(new SubmissionNode(*node.resourceNode));
    node.schedulerType = sched;
    switch (node.schedulerType) {
    case PeerCompNode::SimpleSchedulerClass:
        node.scheduler.reset(new SimpleScheduler(*node.resourceNode));
        node.dispatcher.reset(new SimpleDispatcher(*node.structureNode));
        break;
    case PeerCompNode::FCFSSchedulerClass:
        node.scheduler.reset(new FCFSScheduler(*node.resourceNode));
        node.dispatcher.reset(new QueueBalancingDispatcher(*node.structureNode));
        break;
    case PeerCompNode::EDFSchedulerClass:
        node.scheduler.reset(new EDFScheduler(*node.resourceNode));
        node.dispatcher.reset(new DeadlineDispatcher(*node.structureNode));
        break;
    case PeerCompNode::MinStretchSchedulerClass:
        node.scheduler.reset(new MinStretchScheduler(*node.resourceNode));
        node.minStretchDisp.reset(new MinStretchDispatcher(*node.structureNode));
        break;
    default:
        //node.serializeState(*ia);
        break;
    }
}


int PeerCompNode::processFunction(int argc, char * argv[]) {
    // NOTE: Beware concurrency!!!!
    // Initialise the current node
    Simulator & sim = Simulator::getInstance();
    PeerCompNode & node = Simulator::getCurrentNode();
    PeerCompNodeFactory::getInstance().setupNode(node);
    LogMsg("Sim.Process", DEBUG) << "Peer running at " << MSG_host_get_name(node.getHost()) << " with address " << node.getLocalAddress();
    
    // Message loop
    while(sim.doContinue()) {
        // Event loop: receive messages from the simulator and treat them
        m_task_t task = NULL;
        double timeout = node.getTimeout();
        msg_comm_t comm = MSG_task_irecv(&task, node.getMailbox().c_str());
        if (MSG_comm_wait(comm, timeout) == MSG_OK) {
            MSG_comm_destroy(comm);
            BasicMsg * msg = static_cast<BasicMsg *>(MSG_task_get_data(task));
            const CommAddress & src = static_cast<PeerCompNode *>(MSG_host_get_data(MSG_task_get_source(task)))->getLocalAddress();
            node.enqueueMessage(src, boost::shared_ptr<BasicMsg>(msg));
            while (!node.messageQueue.empty())
                node.processNextMessage();
        } else {
            MSG_comm_destroy(comm);
            // Check timers
            node.checkExpired();
        }
    }
    
    node.finish();
    return 0;
}


unsigned long int PeerCompNode::getMsgSize(BasicMsg * msg) {
    unsigned long int size = 0;
    std::stringstream ss;
    portable_binary_oarchive oa(ss);
    oa << msg;
    size = ss.tellp();
    return size;
}


void PeerCompNode::checkExpired() {
    Time ct = Time::getCurrentTime();
    while (!timerList.empty()) {
        if (timerList.front().timeout <= ct) {
            enqueueMessage(localAddress, timerList.front().msg);
            timerList.pop_front();
        } else break;
    }
}


void PeerCompNode::setAddressAndHost(unsigned int addr, m_host_t host) {
    localAddress = CommAddress(addr, ConfigurationManager::getInstance().getPort());
    simHost = host;
    ostringstream oss;
    oss << localAddress;
    mailbox = oss.str();
}


void PeerCompNode::finish() {
    // Gracely terminate the services
    structureNode.reset();
    resourceNode.reset();
    submissionNode.reset();
    scheduler.reset();
    dispatcher.reset();
    minStretchDisp.reset();
}


// shared_ptr<AvailabilityInformation> PeerCompNode::getBranchInfo() const {
//     if (schedulerType == MinStretchSchedulerClass)
//         return minStretchDisp->getBranchInfo();
//     else return dispatcher->getBranchInfo();
// }
// 
// 
// shared_ptr<AvailabilityInformation> PeerCompNode::getChildInfo(const CommAddress & child) const {
//     if (schedulerType == MinStretchSchedulerClass)
//         return minStretchDisp->getChildInfo(child);
//     else return dispatcher->getChildInfo(child);
// }
// 
// 
// unsigned int PeerCompNode::getSNLevel() const {
//     if (!structureNode->inNetwork())
//         return Simulator::getInstance().getNode(resourceNode->getFather().getIPNum()).getSNLevel() + 1;
//     else if (structureNode->getFather() != CommAddress())
//         return Simulator::getInstance().getNode(structureNode->getFather().getIPNum()).getSNLevel() + 1;
//     else return 0;
// }
// 
// 
// void PeerCompNode::showRecursive(log4cpp::Priority::Value prio, unsigned int level, const string & prefix) {
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
//             PeerCompNode & childNode = Simulator::getInstance().getNode(childAddr);
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
// void PeerCompNode::showPartialTree(bool isBranch, log4cpp::Priority::Value prio) {
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
//                 PeerCompNode & childNode = Simulator::getInstance().getNode(childAddr);
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
// unsigned int PeerCompNode::getRoot() const {
//     const CommAddress & father = structureNode->inNetwork() ? structureNode->getFather() : resourceNode->getFather();
//     if (father != CommAddress()) return Simulator::getInstance().getNode(father.getIPNum()).getRoot();
//     else return localAddress.getIPNum();
// }
// 
// 
// void PeerCompNode::showTree(log4cpp::Priority::Value prio) {
//     if (treeCat.isPriorityEnabled(prio)) {
//         treeCat << prio << "Final tree:";
//         for (unsigned int i = 0; i < Simulator::getInstance().getNumNodes(); i++) {
//             unsigned int rootIP = Simulator::getInstance().getNode(i).getRoot();
//             PeerCompNode & root = Simulator::getInstance().getNode(rootIP);
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
// void PeerCompNode::checkTree() {
//     unsigned int root0 = Simulator::getInstance().getNode(0).getRoot();
//     for (unsigned int i = 1; i < Simulator::getInstance().getNumNodes(); i++) {
//         unsigned int root = Simulator::getInstance().getNode(i).getRoot();
//         if (root != root0) {
//             treeCat.errorStream() << "Node " << CommAddress(i, ConfigurationManager::getInstance().getPort()) << " outside main tree";
//         }
//     }
// }
// 
// 
// void PeerCompNode::saveState(const Properties & property) {
//     std::string outFileName = property("out_file", string(""));
//     if (outFileName != "") {
//         fs::ofstream file;
//         file.exceptions(ios_base::failbit | ios_base::badbit);
//         file.open(outFileName, ios_base::binary);
//         filtering_streambuf<iost::output> out;
//         out.push(iost::zlib_compressor());
//         out.push(file);
//         portable_binary_oarchive oa(out, 0);
//         unsigned int n = Simulator::getInstance().getNumNodes();
//         for (unsigned int i = 0; i < n; i++) {
//             // Save PeerCompNode state
//             Simulator::getInstance().getNode(i).serializeState(oa);
//         }
//     }
// }
// 
// 
// void PeerCompNode::serializeState(portable_binary_oarchive & ar) {
//     ar << schedulerType << power << mem << disk;
//     structureNode->serializeState(ar);
//     resourceNode->serializeState(ar);
//     switch (schedulerType) {
//     case SimpleSchedulerClass:
//         static_cast<SimpleDispatcher &>(*dispatcher).serializeState(ar);
//         break;
//     case FCFSSchedulerClass:
//         static_cast<QueueBalancingDispatcher &>(*dispatcher).serializeState(ar);
//         break;
//     case EDFSchedulerClass:
//         static_cast<DeadlineDispatcher &>(*dispatcher).serializeState(ar);
//         break;
//     case MinStretchSchedulerClass:
//         minStretchDisp->serializeState(ar);
//         break;
//     default:
//         break;
//     }
// }
// 
// 
// void PeerCompNode::serializeState(portable_binary_iarchive & ar) {
//     ar >> schedulerType >> power >> mem >> disk;
//     structureNode->serializeState(ar);
//     resourceNode->serializeState(ar);
//     switch (schedulerType) {
//     case SimpleSchedulerClass: {
//         SimpleDispatcher * d = new SimpleDispatcher(*structureNode);
//         d->serializeState(ar);
//         dispatcher.reset(d);
//         scheduler.reset(new SimpleScheduler(*resourceNode));
//     }
//     break;
//     case FCFSSchedulerClass: {
//         QueueBalancingDispatcher * d = new QueueBalancingDispatcher(*structureNode);
//         d->serializeState(ar);
//         dispatcher.reset(d);
//         scheduler.reset(new FCFSScheduler(*resourceNode));
//     }
//     break;
//     case EDFSchedulerClass: {
//         DeadlineDispatcher * d = new DeadlineDispatcher(*structureNode);
//         d->serializeState(ar);
//         dispatcher.reset(d);
//         scheduler.reset(new EDFScheduler(*resourceNode));
//     }
//     break;
//     case MinStretchSchedulerClass: {
//         MinStretchDispatcher * d = new MinStretchDispatcher(*structureNode);
//         d->serializeState(ar);
//         minStretchDisp.reset(d);
//         scheduler.reset(new MinStretchScheduler(*resourceNode));
//     }
//     break;
//     default:
//         break;
//     }
// }
// 
// 
// class MemoryOutArchive {
//     vector<void *>::iterator ptr;
// public:
//     typedef boost::mpl::bool_<false> is_loading;
//     MemoryOutArchive(vector<void *>::iterator o) : ptr(o) {}
//     template<class T> MemoryOutArchive & operator<<(T & o) {
//         *ptr++ = &o;
//         return *this;
//     }
//     template<class T> MemoryOutArchive & operator&(T & o) {
//         return operator<<(o);
//     }
// };
// 
// 
// class MemoryInArchive {
//     vector<void *>::iterator ptr;
// public:
//     typedef boost::mpl::bool_<true> is_loading;
//     MemoryInArchive(vector<void *>::iterator o) : ptr(o) {}
//     template<class T> MemoryInArchive & operator>>(T & o) {
//         o = *static_cast<T *>(*ptr++);
//         return *this;
//     }
//     template<class T> MemoryInArchive & operator&(T & o) {
//         return operator>>(o);
//     }
// };
// 
// 
// void PeerCompNode::generateRNode(uint32_t rfather) {
//     vector<void *> v(10);
//     MemoryOutArchive oa(v.begin());
//     // Generate ResourceNode information
//     shared_ptr<ZoneDescription> rzone(new ZoneDescription);
//     CommAddress father(rfather, ConfigurationManager::getInstance().getPort());
//     rzone->setMinAddress(father);
//     rzone->setMaxAddress(father);
//     uint64_t seq = 0;
//     oa << father << seq << rzone << rzone;
//     MemoryInArchive ia(v.begin());
//     resourceNode->serializeState(ia);
// }
// 
// 
// void PeerCompNode::generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, int level) {
//     Simulator & sim = Simulator::getInstance();
//     vector<void *> v(10);
//     MemoryOutArchive oa(v.begin());
//     // Generate StructureNode information
//     shared_ptr<ZoneDescription> zone(new ZoneDescription);
//     CommAddress father;
//     if (sfather != localAddress.getIPNum())
//         father = CommAddress(sfather, ConfigurationManager::getInstance().getPort());
//     list<shared_ptr<TransactionalZoneDescription> > subZones;
//     shared_ptr<ZoneDescription> zone1, zone2;
//     if (level == 0) {
//         zone1.reset(new ZoneDescription);
//         zone1->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//         zone1->setMaxAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//         zone2.reset(new ZoneDescription);
//         zone2->setMinAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
//         zone2->setMaxAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
//         zone->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//         zone->setMaxAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
//     } else {
//         zone1.reset(new ZoneDescription(*sim.getNode(schild1).structureNode->getZoneDesc()));
//         zone2.reset(new ZoneDescription(*sim.getNode(schild2).structureNode->getZoneDesc()));
//         zone->setMinAddress(zone1->getMinAddress());
//         zone->setMaxAddress(zone2->getMaxAddress());
//     }
//     subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
//     subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
//     subZones.front()->setLink(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//     subZones.front()->setZone(zone1);
//     subZones.front()->commit();
//     subZones.back()->setLink(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
//     subZones.back()->setZone(zone2);
//     subZones.back()->commit();
//     uint64_t seq = 0;
//     unsigned int m = 2;
//     int state = StructureNode::ONLINE;
//     oa << state << m << level << zone << zone << father << seq << subZones;
//     MemoryInArchive ia(v.begin());
//     structureNode->serializeState(ia);
// 
//     // Generate Dispatcher state
//     switch (schedulerType) {
//     case SimpleSchedulerClass:
//         generateDispatcher<SimpleDispatcher>(father, schild1, schild2, level);
//         break;
//     case FCFSSchedulerClass:
//         generateDispatcher<QueueBalancingDispatcher>(father, schild1, schild2, level);
//         break;
//     case EDFSchedulerClass:
//         generateDispatcher<DeadlineDispatcher>(father, schild1, schild2, level);
//         break;
//     case MinStretchSchedulerClass:
//         generateDispatcher<MinStretchDispatcher>(father, schild1, schild2, level);
// //  {
// //   vector<void *> vv(10);
// //   MemoryOutArchive oaa(vv.begin());
// //   vector<MinStretchDispatcher::Child> children(2);
// //   children[0].first = CommAddress(schild1, ConfigurationManager::getInstance().getPort());
// //   children[1].first = CommAddress(schild2, ConfigurationManager::getInstance().getPort());
// //   if (level == 0) {
// //    children[0].second.reset(static_cast<StretchInformation *>(sim.getNode(schild1).scheduler->getAvailability().clone()));
// //    children[1].second.reset(static_cast<StretchInformation *>(sim.getNode(schild2).scheduler->getAvailability().clone()));
// //   } else {
// //    children[0].second = sim.getNode(schild1).minStretchDisp->getBranchInfo();
// //    children[1].second = sim.getNode(schild2).minStretchDisp->getBranchInfo();
// //   }
// //   shared_ptr<StretchInformation> avail(children[0].second->clone());
// //   avail->join(*children[1].second);
// //   avail->reduce();
// //   shared_ptr<StretchInformation> zero(new StretchInformation(0, 0, 0, 0, 0));
// //   vector<shared_ptr<StretchInformation> > ntfChildren(2, zero);
// //   oaa << children << zero << avail << ntfChildren << avail;
// //   MemoryInArchive iaa(vv.begin());
// //   minStretchDisp->serializeState(iaa);
// //  }
//         break;
//     default:
//         break;
//     }
// }
// 
// 
// template <class T>
// void PeerCompNode::generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, int level) {
//     Simulator & sim = Simulator::getInstance();
//     vector<void *> vv(10);
//     MemoryOutArchive oaa(vv.begin());
//     vector<typename T::Link> children(2);
//     children[0].addr = CommAddress(schild1, ConfigurationManager::getInstance().getPort());
//     children[1].addr = CommAddress(schild2, ConfigurationManager::getInstance().getPort());
//     if (level == 0) {
//         children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).scheduler->getAvailability().clone()));
//         children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).scheduler->getAvailability().clone()));
//     } else {
//         children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).dispatcher->getBranchInfo()->clone()));
//         children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).dispatcher->getBranchInfo()->clone()));
//     }
//     children[0].availInfo->reduce();
//     children[1].availInfo->reduce();
//     typename T::Link fatherLink;
//     fatherLink.addr = father;
//     oaa << fatherLink << children;
//     MemoryInArchive iaa(vv.begin());
//     static_cast<T &>(*dispatcher).serializeState(iaa);
// }
// 
// 
// void PeerCompNode::generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level) {
//     Simulator & sim = Simulator::getInstance();
//     vector<void *> v(10);
//     MemoryOutArchive oa(v.begin());
//     // Generate StructureNode information
//     shared_ptr<ZoneDescription> zone(new ZoneDescription);
//     CommAddress father;
//     if (sfather != localAddress.getIPNum())
//         father = CommAddress(sfather, ConfigurationManager::getInstance().getPort());
//     list<shared_ptr<TransactionalZoneDescription> > subZones;
//     shared_ptr<ZoneDescription> zone1, zone2, zone3;
//     if (level == 0) {
//         zone1.reset(new ZoneDescription);
//         zone1->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//         zone1->setMaxAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//         zone2.reset(new ZoneDescription);
//         zone2->setMinAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
//         zone2->setMaxAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
//         zone3.reset(new ZoneDescription);
//         zone3->setMinAddress(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
//         zone3->setMaxAddress(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
//         zone->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//         zone->setMaxAddress(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
//     } else {
//         zone1.reset(new ZoneDescription(*sim.getNode(schild1).structureNode->getZoneDesc()));
//         zone2.reset(new ZoneDescription(*sim.getNode(schild2).structureNode->getZoneDesc()));
//         zone3.reset(new ZoneDescription(*sim.getNode(schild3).structureNode->getZoneDesc()));
//         zone->setMinAddress(zone1->getMinAddress());
//         zone->setMaxAddress(zone3->getMaxAddress());
//     }
//     subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
//     subZones.back()->setLink(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
//     subZones.back()->setZone(zone1);
//     subZones.back()->commit();
//     subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
//     subZones.back()->setLink(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
//     subZones.back()->setZone(zone2);
//     subZones.back()->commit();
//     subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
//     subZones.back()->setLink(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
//     subZones.back()->setZone(zone3);
//     subZones.back()->commit();
//     unsigned int m = 2;
//     uint64_t seq = 0;
//     int state = StructureNode::ONLINE;
//     oa << state << m << level << zone << zone << father << seq << subZones;
//     MemoryInArchive ia(v.begin());
//     structureNode->serializeState(ia);
// 
//     // Generate Dispatcher state
//     switch (schedulerType) {
//     case SimpleSchedulerClass:
//         generateDispatcher<SimpleDispatcher>(father, schild1, schild2, schild3, level);
//         break;
//     case FCFSSchedulerClass:
//         generateDispatcher<QueueBalancingDispatcher>(father, schild1, schild2, schild3, level);
//         break;
//     case EDFSchedulerClass:
//         generateDispatcher<DeadlineDispatcher>(father, schild1, schild2, schild3, level);
//         break;
//     case MinStretchSchedulerClass:
//         generateDispatcher<MinStretchDispatcher>(father, schild1, schild2, schild3, level);
// //  {
// //   vector<void *> vv(10);
// //   MemoryOutArchive oaa(vv.begin());
// //   vector<MinStretchDispatcher::Child> children(3);
// //   children[0].first = CommAddress(schild1, ConfigurationManager::getInstance().getPort());
// //   children[1].first = CommAddress(schild2, ConfigurationManager::getInstance().getPort());
// //   children[2].first = CommAddress(schild3, ConfigurationManager::getInstance().getPort());
// //   if (level == 0) {
// //    children[0].second.reset(static_cast<StretchInformation *>(sim.getNode(schild1).scheduler->getAvailability().clone()));
// //    children[1].second.reset(static_cast<StretchInformation *>(sim.getNode(schild2).scheduler->getAvailability().clone()));
// //    children[2].second.reset(static_cast<StretchInformation *>(sim.getNode(schild3).scheduler->getAvailability().clone()));
// //   } else {
// //    children[0].second = sim.getNode(schild1).minStretchDisp->getBranchInfo();
// //    children[1].second = sim.getNode(schild2).minStretchDisp->getBranchInfo();
// //    children[2].second = sim.getNode(schild3).minStretchDisp->getBranchInfo();
// //   }
// //   shared_ptr<StretchInformation> avail(children[0].second->clone());
// //   avail->join(*children[1].second);
// //   avail->join(*children[2].second);
// //   avail->reduce();
// //   shared_ptr<StretchInformation> zero(new StretchInformation(0, 0, 0, 0, 0));
// //   vector<shared_ptr<StretchInformation> > ntfChildren(3, zero);
// //   oaa << children << zero << avail << ntfChildren << avail;
// //   MemoryInArchive iaa(vv.begin());
// //   minStretchDisp->serializeState(iaa);
// //  }
//         break;
//     default:
//         break;
//     }
// }
// 
// 
// template <class T>
// void PeerCompNode::generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level) {
//     Simulator & sim = Simulator::getInstance();
//     vector<void *> vv(10);
//     MemoryOutArchive oaa(vv.begin());
//     vector<typename T::Link> children(3);
//     children[0].addr = CommAddress(schild1, ConfigurationManager::getInstance().getPort());
//     children[1].addr = CommAddress(schild2, ConfigurationManager::getInstance().getPort());
//     children[2].addr = CommAddress(schild3, ConfigurationManager::getInstance().getPort());
//     if (level == 0) {
//         children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).scheduler->getAvailability().clone()));
//         children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).scheduler->getAvailability().clone()));
//         children[2].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild3).scheduler->getAvailability().clone()));
//     } else {
//         children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).dispatcher->getBranchInfo()->clone()));
//         children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).dispatcher->getBranchInfo()->clone()));
//         children[2].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild3).dispatcher->getBranchInfo()->clone()));
//     }
//     children[0].availInfo->reduce();
//     children[1].availInfo->reduce();
//     children[2].availInfo->reduce();
//     typename T::Link fatherLink;
//     fatherLink.addr = father;
//     oaa << fatherLink << children;
//     MemoryInArchive iaa(vv.begin());
//     static_cast<T &>(*dispatcher).serializeState(iaa);
// }
