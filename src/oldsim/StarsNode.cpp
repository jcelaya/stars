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

#include <sstream>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
namespace iost = boost::iostreams;
#include <log4cpp/Category.hh>
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
using namespace std;
using namespace boost::iostreams;
using namespace boost::posix_time;
using namespace boost::gregorian;
using boost::shared_ptr;
using boost::scoped_ptr;


static log4cpp::Category & treeCat = log4cpp::Category::getInstance("Sim.Tree");


class SimExecutionEnvironment : public Scheduler::ExecutionEnvironment {
    StarsNode & node;
public:
    SimExecutionEnvironment(StarsNode & n) : node(n) {}

    double getAveragePower() const { return node.getAveragePower(); }

    unsigned long int getAvailableMemory() const { return node.getAvailableMemory(); }

    unsigned long int getAvailableDisk() const { return node.getAvailableDisk(); }

    shared_ptr<Task> createTask(CommAddress o, long int reqId, unsigned int ctid, const TaskDescription & d) const {
        return shared_ptr<Task>(new SimTask(o, reqId, ctid, d));
    }
};


// Rewritten methods
// Execution environment
Scheduler::ExecutionEnvironmentImpl::ExecutionEnvironmentImpl()
    : impl(new SimExecutionEnvironment(Simulator::getInstance().getCurrentNode())) {}


// CommLayer
CommLayer::CommLayer() : exitSignaled(false) {}


CommLayer & CommLayer::getInstance() {
    // Always called from STaRSLib!!
    return Simulator::getInstance().getCurrentNode();
}


unsigned int CommLayer::sendMessage(const CommAddress & dst, BasicMsg * msg) {
    return Simulator::getInstance().sendMessage(localAddress.getIPNum(), dst.getIPNum(), shared_ptr<BasicMsg>(msg));
}


int CommLayer::setTimerImpl(Time time, shared_ptr<BasicMsg> msg) {
    return Simulator::getInstance().setTimer(localAddress.getIPNum(), time, msg);
}


void CommLayer::cancelTimer(int timerId) {
    Simulator::getInstance().cancelTimer(timerId);
}


// Time
Time Time::getCurrentTime() {
    return Simulator::getInstance().getCurrentTime();
}


std::ostream & operator<<(std::ostream & os, const Time & r) {
    return os << Duration(r.getRawDate());
}


// Configuration object
struct StarsNodeConfiguration {
    int minCPU;
    int maxCPU;
    int stepCPU;
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
        outFileName = property("out_file", string(""));
        string s = property("scheduler", string(""));
        if (s == "DS") sched = StarsNode::EDFSchedulerClass;
        else if (s == "MS") sched = StarsNode::MSSchedulerClass;
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
            // Create directories if needed
            fs::path parentDir = fs::absolute(outFileName).parent_path();
            if (!fs::exists(parentDir))
                 fs::create_directories(parentDir);
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
        SlownessInformation::setNumClusters(clustersBase * clustersBase * clustersBase);
    }
    TimeConstraintInfo::setNumRefPoints(property("tci_ref_points", 8U));
    SlownessInformation::setNumPieces(property("si_pieces", 64U));
    SimAppDatabase::reset();
    StarsNodeConfiguration::getInstance().setup(property);
}


void StarsNode::setup(unsigned int addr) {
    localAddress = CommAddress(addr, ConfigurationManager::getInstance().getPort());
    StarsNodeConfiguration & cfg = StarsNodeConfiguration::getInstance();
    // Execution power follows discretized pareto distribution, with k=1
    power = Simulator::discretePareto(cfg.minCPU, cfg.maxCPU, cfg.stepCPU, 1.0);
    mem = Simulator::uniform(cfg.minMem, cfg.maxMem, cfg.stepMem);
    disk = Simulator::uniform(cfg.minDisk, cfg.maxDisk, cfg.stepDisk);
    schedulerType = cfg.sched;

    createServices();
    // Load service state if needed
    if (cfg.inFile.is_open()) {
        unpackState(cfg.in);
    }
}


void StarsNode::finish() {
    StarsNodeConfiguration & cfg = StarsNodeConfiguration::getInstance();
    if (cfg.outFile.is_open()) {
        packState(cfg.out);
    }
    // Very important to set current node before destroying services
    Simulator::getInstance().setCurrentNode(getLocalAddress().getIPNum());
    // Gracely terminate the services
    for (int i = services.size() - 1; i >= 0; --i)
        delete services[i];
    services.clear();
}


void StarsNode::createServices() {
    services.push_back(new StructureNode(2));
    services.push_back(new ResourceNode(getS()));
    services.push_back(new SubmissionNode(getE()));
    switch (schedulerType) {
        case StarsNode::FCFSSchedulerClass:
            services.push_back(new FCFSScheduler(getE()));
            services.push_back(new QueueBalancingDispatcher(getS()));
            break;
        case StarsNode::EDFSchedulerClass:
            services.push_back(new EDFScheduler(getE()));
            services.push_back(new DeadlineDispatcher(getS()));
            break;
        case StarsNode::MSSchedulerClass:
            services.push_back(new MinSlownessScheduler(getE()));
            services.push_back(new MinSlownessDispatcher(getS()));
            break;
        default:
            services.push_back(new SimpleScheduler(getE()));
            services.push_back(new SimpleDispatcher(getS()));
            break;
    }
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
    getS().serializeState(ar);
    getE().serializeState(ar);
    switch (schedulerType) {
        case SimpleSchedulerClass:
            static_cast<SimpleDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case FCFSSchedulerClass:
            static_cast<QueueBalancingDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case EDFSchedulerClass:
            static_cast<DeadlineDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case MSSchedulerClass:
            static_cast<MinSlownessDispatcher &>(*services[Disp]).serializeState(ar);
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
    getS().serializeState(ar);
    getE().serializeState(ar);
    switch (schedulerType) {
        case SimpleSchedulerClass:
            static_cast<SimpleDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case FCFSSchedulerClass:
            static_cast<QueueBalancingDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case EDFSchedulerClass:
            static_cast<DeadlineDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case MSSchedulerClass:
            static_cast<MinSlownessDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        default:
            break;
    }
}


shared_ptr<AvailabilityInformation> StarsNode::getBranchInfo() const {
    return getDispatcher().getBranchInfo();
}


shared_ptr<AvailabilityInformation> StarsNode::getChildInfo(const CommAddress & child) const {
    return getDispatcher().getChildInfo(child);
}


unsigned int StarsNode::getSNLevel() const {
    if (!getS().inNetwork())
        return Simulator::getInstance().getNode(getE().getFather().getIPNum()).getSNLevel() + 1;
    else if (getS().getFather() != CommAddress())
        return Simulator::getInstance().getNode(getS().getFather().getIPNum()).getSNLevel() + 1;
    else return 0;
}


void StarsNode::showRecursive(log4cpp::Priority::Value prio, unsigned int level, const string & prefix) {
    shared_ptr<AvailabilityInformation> info = getBranchInfo();
    if (info.get())
        treeCat << prio << prefix << "S@" << localAddress << ": " << getS() << ' ' << *info;
    else
        treeCat << prio << prefix << "S@" << localAddress << ": " << getS() << " ?";
    if (level) {
        for (unsigned int i = 0; i < getS().getNumChildren(); i++) {
            bool last = i == getS().getNumChildren() - 1;
            stringstream father, child;
            father << prefix << "  " << ((last) ? '\\' : '|') << "- ";
            child << prefix << "  " << ((last) ? ' ' : '|') << "  ";

            uint32_t childAddr;
            if (getS().getSubZone(i)->getLink() != CommAddress())
                childAddr = getS().getSubZone(i)->getLink().getIPNum();
            else childAddr = getS().getSubZone(i)->getNewLink().getIPNum();
            StarsNode & childNode = Simulator::getInstance().getNode(childAddr);
            info = getChildInfo(CommAddress(childAddr, ConfigurationManager::getInstance().getPort()));
            if (info.get())
                treeCat << prio << father.str() << *getS().getSubZone(i) << ' ' << *info;
            else
                treeCat << prio << father.str() << *getS().getSubZone(i) << " ?";
            if (!getS().isRNChildren()) {
                childNode.showRecursive(prio, level - 1, child.str());
            } else {
                treeCat << prio << child.str() << "R@" << CommAddress(childAddr, ConfigurationManager::getInstance().getPort()) << ": " << childNode.getE() << " "
                        << childNode << ' ' << childNode.getScheduler().getAvailability();
            }
        }
    }
}


void StarsNode::showPartialTree(bool isBranch, log4cpp::Priority::Value prio) {
    if (treeCat.isPriorityEnabled(prio)) {
        StructureNode * father = NULL;
        uint32_t fatherIP = 0;
        if (isBranch) {
            if (getS().getFather() != CommAddress()) {
                fatherIP = getS().getFather().getIPNum();
                father = &Simulator::getInstance().getNode(fatherIP).getS();
            }
        }
        else {
            if (getE().getFather() != CommAddress()) {
                fatherIP = getS().getFather().getIPNum();
                father = &Simulator::getInstance().getNode(fatherIP).getS();
            } else {
                // This may be an error...
                treeCat.warnStream() << "Resource node without father???: R@" << localAddress << ": " << getE();
                return;
            }
        }

        if (father != NULL) {
            treeCat << prio << "S@" << Simulator::getInstance().getNode(fatherIP).getLocalAddress() << ": " << *father;
            for (unsigned int i = 0; i < father->getNumChildren(); i++) {
                bool last = i == father->getNumChildren() - 1;
                treeCat << prio << (last ? "  \\- " : "  |- ") << *father->getSubZone(i);
                uint32_t childAddr;
                if (father->getSubZone(i)->getLink() != CommAddress())
                    childAddr = father->getSubZone(i)->getLink().getIPNum();
                else childAddr = father->getSubZone(i)->getNewLink().getIPNum();
                StarsNode & childNode = Simulator::getInstance().getNode(childAddr);
                if (!father->isRNChildren()) {
                    childNode.showRecursive(prio, childAddr == localAddress.getIPNum() ? 1 : 0, (last ? "     " : "  |  "));
                } else {
                    treeCat << prio << (last ? "     " : "  |  ") << "R@" << childNode.localAddress << ": " << childNode.getE();
                }
            }
        }
        else showRecursive(prio, 1);
    }
}


unsigned int StarsNode::getRoot() const {
    const CommAddress & father = getS().inNetwork() ? getS().getFather() : getE().getFather();
    if (father != CommAddress()) return Simulator::getInstance().getNode(father.getIPNum()).getRoot();
    else return localAddress.getIPNum();
}


void StarsNode::showTree(log4cpp::Priority::Value prio) {
    if (treeCat.isPriorityEnabled(prio)) {
        treeCat << prio << "Final tree:";
        for (unsigned int i = 0; i < Simulator::getInstance().getNumNodes(); i++) {
            unsigned int rootIP = Simulator::getInstance().getNode(i).getRoot();
            StarsNode & root = Simulator::getInstance().getNode(rootIP);
            if (root.getS().inNetwork()) {
                root.showRecursive(prio, -1);
                treeCat << prio << "";
                treeCat << prio << "";
                break;
            }
        }
    }
}


void StarsNode::checkTree() {
    unsigned int root0 = Simulator::getInstance().getNode(0).getRoot();
    for (unsigned int i = 1; i < Simulator::getInstance().getNumNodes(); i++ ) {
        unsigned int root = Simulator::getInstance().getNode(i).getRoot();
        if (root != root0) {
            treeCat.errorStream() << "Node " << CommAddress(i, ConfigurationManager::getInstance().getPort()) << " outside main tree";
        }
    }
}


class MemoryOutArchive {
    vector<void *>::iterator ptr;
public:
    typedef boost::mpl::bool_<false> is_loading;
    MemoryOutArchive(vector<void *>::iterator o) : ptr(o) {}
    template<class T> MemoryOutArchive & operator<<(T & o) { *ptr++ = &o; return *this; }
    template<class T> MemoryOutArchive & operator&(T & o) { return operator<<(o); }
};


class MemoryInArchive {
    vector<void *>::iterator ptr;
public:
    typedef boost::mpl::bool_<true> is_loading;
    MemoryInArchive(vector<void *>::iterator o) : ptr(o) {}
    template<class T> MemoryInArchive & operator>>(T & o) { o = *static_cast<T *>(*ptr++); return *this; }
    template<class T> MemoryInArchive & operator&(T & o) { return operator>>(o); }
};


void StarsNode::generateRNode(uint32_t rfather) {
    vector<void *> v(10);
    MemoryOutArchive oa(v.begin());
    // Generate ResourceNode information
    CommAddress father(rfather, ConfigurationManager::getInstance().getPort());
    uint64_t seq = 0;
    oa << father << seq;
    MemoryInArchive ia(v.begin());
    getE().serializeState(ia);
}


void StarsNode::generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, int level) {
    Simulator & sim = Simulator::getInstance();
    vector<void *> v(10);
    MemoryOutArchive oa(v.begin());
    // Generate StructureNode information
    shared_ptr<ZoneDescription> zone(new ZoneDescription);
    CommAddress father;
    if (sfather != localAddress.getIPNum())
        father = CommAddress(sfather, ConfigurationManager::getInstance().getPort());
    list<shared_ptr<TransactionalZoneDescription> > subZones;
    shared_ptr<ZoneDescription> zone1, zone2;
    if (level == 0) {
        zone1.reset(new ZoneDescription);
        zone1->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        zone1->setMaxAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        zone2.reset(new ZoneDescription);
        zone2->setMinAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
        zone2->setMaxAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
        zone->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        zone->setMaxAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
    } else {
        zone1.reset(new ZoneDescription(*sim.getNode(schild1).getS().getZoneDesc()));
        zone2.reset(new ZoneDescription(*sim.getNode(schild2).getS().getZoneDesc()));
        zone->setMinAddress(zone1->getMinAddress());
        zone->setMaxAddress(zone2->getMaxAddress());
    }
    subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
    subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
    subZones.front()->setLink(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
    subZones.front()->setZone(zone1);
    subZones.front()->commit();
    subZones.back()->setLink(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
    subZones.back()->setZone(zone2);
    subZones.back()->commit();
    uint64_t seq = 0;
    unsigned int m = 2;
    int state = StructureNode::ONLINE;
    oa << state << m << level << zone << zone << father << seq<< subZones;
    MemoryInArchive ia(v.begin());
    getS().serializeState(ia);

    // Generate Dispatcher state
    switch (schedulerType) {
    case SimpleSchedulerClass:
        generateDispatcher<SimpleDispatcher>(father, schild1, schild2, level);
        break;
    case FCFSSchedulerClass:
        generateDispatcher<QueueBalancingDispatcher>(father, schild1, schild2, level);
        break;
    case EDFSchedulerClass:
        generateDispatcher<DeadlineDispatcher>(father, schild1, schild2, level);
        break;
    case MSSchedulerClass:
        generateDispatcher<MinSlownessDispatcher>(father, schild1, schild2, level);
        break;
    default:
        break;
    }
}


template <class T>
void StarsNode::generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, int level) {
    Simulator & sim = Simulator::getInstance();
    vector<void *> vv(20);
    MemoryOutArchive oaa(vv.begin());
    vector<typename T::Link> children(2);
    children[0].addr = CommAddress(schild1, ConfigurationManager::getInstance().getPort());
    children[1].addr = CommAddress(schild2, ConfigurationManager::getInstance().getPort());
    if (level == 0) {
        children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).getScheduler().getAvailability().clone()));
        children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).getScheduler().getAvailability().clone()));
    } else {
        children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).getDispatcher().getBranchInfo()->clone()));
        children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).getDispatcher().getBranchInfo()->clone()));
    }
    children[0].availInfo->reduce();
    children[1].availInfo->reduce();
    typename T::Link fatherLink;
    fatherLink.addr = father;
    fatherLink.serializeState(oaa);
    size_t size = 2;
    oaa << size;
    children[0].serializeState(oaa);
    children[1].serializeState(oaa);
    MemoryInArchive iaa(vv.begin());
    static_cast<T &>(getDispatcher()).serializeState(iaa);
    static_cast<T &>(getDispatcher()).recomputeInfo();
}


void StarsNode::generateSNode(uint32_t sfather, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level) {
    Simulator & sim = Simulator::getInstance();
    vector<void *> v(10);
    MemoryOutArchive oa(v.begin());
    // Generate StructureNode information
    shared_ptr<ZoneDescription> zone(new ZoneDescription);
    CommAddress father;
    if (sfather != localAddress.getIPNum())
        father= CommAddress(sfather, ConfigurationManager::getInstance().getPort());
    list<shared_ptr<TransactionalZoneDescription> > subZones;
    shared_ptr<ZoneDescription> zone1, zone2, zone3;
    if (level == 0) {
        zone1.reset(new ZoneDescription);
        zone1->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        zone1->setMaxAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        zone2.reset(new ZoneDescription);
        zone2->setMinAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
        zone2->setMaxAddress(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
        zone3.reset(new ZoneDescription);
        zone3->setMinAddress(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
        zone3->setMaxAddress(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
        zone->setMinAddress(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        zone->setMaxAddress(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
    } else {
        zone1.reset(new ZoneDescription(*sim.getNode(schild1).getS().getZoneDesc()));
        zone2.reset(new ZoneDescription(*sim.getNode(schild2).getS().getZoneDesc()));
        zone3.reset(new ZoneDescription(*sim.getNode(schild3).getS().getZoneDesc()));
        zone->setMinAddress(zone1->getMinAddress());
        zone->setMaxAddress(zone3->getMaxAddress());
    }
    subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
    subZones.back()->setLink(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
    subZones.back()->setZone(zone1);
    subZones.back()->commit();
    subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
    subZones.back()->setLink(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
    subZones.back()->setZone(zone2);
    subZones.back()->commit();
    subZones.push_back(shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
    subZones.back()->setLink(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
    subZones.back()->setZone(zone3);
    subZones.back()->commit();
    unsigned int m = 2;
    uint64_t seq = 0;
    int state = StructureNode::ONLINE;
    oa << state << m << level << zone << zone << father << seq << subZones;
    MemoryInArchive ia(v.begin());
    getS().serializeState(ia);

    // Generate Dispatcher state
    switch (schedulerType) {
    case SimpleSchedulerClass:
        generateDispatcher<SimpleDispatcher>(father, schild1, schild2, schild3, level);
        break;
    case FCFSSchedulerClass:
        generateDispatcher<QueueBalancingDispatcher>(father, schild1, schild2, schild3, level);
        break;
    case EDFSchedulerClass:
        generateDispatcher<DeadlineDispatcher>(father, schild1, schild2, schild3, level);
        break;
    case MSSchedulerClass:
        generateDispatcher<MinSlownessDispatcher>(father, schild1, schild2, schild3, level);
        break;
    default:
        break;
    }
}


template <class T>
void StarsNode::generateDispatcher(const CommAddress & father, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level) {
    Simulator & sim = Simulator::getInstance();
    vector<void *> vv(20);
    MemoryOutArchive oaa(vv.begin());
    vector<typename T::Link> children(3);
    children[0].addr = CommAddress(schild1, ConfigurationManager::getInstance().getPort());
    children[1].addr = CommAddress(schild2, ConfigurationManager::getInstance().getPort());
    children[2].addr = CommAddress(schild3, ConfigurationManager::getInstance().getPort());
    if (level == 0) {
        children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).getScheduler().getAvailability().clone()));
        children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).getScheduler().getAvailability().clone()));
        children[2].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild3).getScheduler().getAvailability().clone()));
    } else {
        children[0].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild1).getDispatcher().getBranchInfo()->clone()));
        children[1].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild2).getDispatcher().getBranchInfo()->clone()));
        children[2].availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(schild3).getDispatcher().getBranchInfo()->clone()));
    }
    children[0].availInfo->reduce();
    children[1].availInfo->reduce();
    children[2].availInfo->reduce();
    typename T::Link fatherLink;
    fatherLink.addr = father;
    fatherLink.serializeState(oaa);
    size_t size = 3;
    oaa << size;
    children[0].serializeState(oaa);
    children[1].serializeState(oaa);
    children[2].serializeState(oaa);
    MemoryInArchive iaa(vv.begin());
    static_cast<T &>(getDispatcher()).serializeState(iaa);
    static_cast<T &>(getDispatcher()).recomputeInfo();
}
