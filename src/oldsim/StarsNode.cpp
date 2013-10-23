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
#include <boost/iostreams/filter/gzip.hpp>
#include <log4cpp/Category.hh>
#include "StarsNode.hpp"
#include "ResourceNode.hpp"
#include "StructureNode.hpp"
#include "SubmissionNode.hpp"
#include "DPScheduler.hpp"
#include "DPDispatcher.hpp"
#include "MMPScheduler.hpp"
#include "MMPDispatcher.hpp"
#include "IBPScheduler.hpp"
#include "IBPDispatcher.hpp"
#include "FSPScheduler.hpp"
#include "FSPDispatcher.hpp"
#include "Simulator.hpp"
#include "Time.hpp"
#include "SimTask.hpp"
#include "Logger.hpp"
using namespace std;
using namespace boost::iostreams;
using namespace boost::posix_time;
using namespace boost::gregorian;
//using boost::shared_ptr;
using boost::scoped_ptr;


class CheckTimersMsg : public BasicMsg {
public:
    MESSAGE_SUBCLASS(CheckTimersMsg);

    EMPTY_MSGPACK_DEFINE();
};
static boost::shared_ptr<CheckTimersMsg> timerMsg(new CheckTimersMsg);


class SimExecutionEnvironment : public Scheduler::ExecutionEnvironment {
    StarsNode & node;
public:
    SimExecutionEnvironment(StarsNode & n) : node(n) {}

    double getAveragePower() const { return node.getAveragePower(); }

    unsigned long int getAvailableMemory() const { return node.getAvailableMemory(); }

    unsigned long int getAvailableDisk() const { return node.getAvailableDisk(); }

    boost::shared_ptr<Task> createTask(CommAddress o, int64_t reqId, unsigned int ctid, const TaskDescription & d) const {
        return boost::shared_ptr<Task>(new SimTask(o, reqId, ctid, d));
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
    return Simulator::getInstance().sendMessage(localAddress.getIPNum(), dst.getIPNum(), boost::shared_ptr<BasicMsg>(msg));
}


// Time
Time Time::getCurrentTime() {
    return Simulator::getInstance().getCurrentTime();
}


int CommLayer::setTimerImpl(Time time, boost::shared_ptr<BasicMsg> msg) {
    Timer t(time, msg);
    t.id <<= 1;
    // Set a new timer if this timer is the first or comes before the first
    if (timerList.empty() || time < timerList.front().timeout) {
        Simulator::getInstance().injectMessage(localAddress.getIPNum(), localAddress.getIPNum(),
            timerMsg, time - Simulator::getInstance().getCurrentTime());
        ++t.id;
    }
    // Add a task to the timer structure
    timerList.push_front(t);
    timerList.sort();
    return t.id >> 1;
}


void CommLayer::cancelTimer(int timerId) {
    // Erase timer in list
    for (std::list<Timer>::iterator it = timerList.begin(); it != timerList.end(); it++) {
        if (it->id >> 1 == timerId) {
            timerList.erase(it);
            break;
        }
    }
}


std::ostream & operator<<(std::ostream & os, const Time & r) {
    return os << Duration(r.getRawDate());
}


// Configuration object
void StarsNode::Configuration::setup(const Properties & property) {
    cpuVar = DiscreteParetoVariable(property("min_cpu", 1000.0), property("max_cpu", 3000.0), property("step_cpu", 200.0), 1.0);
    memVar = DiscreteUniformVariable(property("min_mem", 256), property("max_mem", 4096), property("step_mem", 256));
    diskVar = DiscreteUniformVariable(property("min_disk", 64), property("max_disk", 1000), property("step_disk", 100));
    inFileName = property("in_file", string(""));
    outFileName = property("out_file", string(""));
    string s = property("policy", string(""));
    if (s == "DP") policy = DPolicy;
    else if (s == "FSP") policy = FSPolicy;
    else if (s == "MMP") policy = MMPolicy;
    else policy = IBPolicy;
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


void StarsNode::libStarsConfigure(const Properties & property) {
    ConfigurationManager::getInstance().setUpdateBandwidth(property("update_bw", 1000.0));
    ConfigurationManager::getInstance().setSlownessRatio(property("stretch_ratio", 2.0));
    ConfigurationManager::getInstance().setHeartbeat(property("heartbeat", 300));
    ConfigurationManager::getInstance().setWorkingPath(Simulator::getInstance().getResultDir());
    ConfigurationManager::getInstance().setSubmitRetries(property("submit_retries", 3));
    ConfigurationManager::getInstance().setRequestTimeout(property("request_timeout", 30.0));
    ConfigurationManager::getInstance().setRescheduleTimeout(property("reschedule_timeout", 600.0));
    unsigned int clustersBase = property("avail_clusters_base", 0U);
    if (clustersBase) {
        IBPAvailabilityInformation::setNumClusters(clustersBase * clustersBase);
        MMPAvailabilityInformation::setNumClusters(clustersBase * clustersBase * clustersBase * clustersBase);
        DPAvailabilityInformation::setNumClusters(clustersBase * clustersBase * clustersBase);
        FSPAvailabilityInformation::setNumClusters(clustersBase * clustersBase * clustersBase);
    } else {
        unsigned int clusters = property("avail_clusters", 20U);
        if (clusters) {
            IBPAvailabilityInformation::setNumClusters(clusters);
            MMPAvailabilityInformation::setNumClusters(clusters);
            DPAvailabilityInformation::setNumClusters(clusters);
            FSPAvailabilityInformation::setNumClusters(clusters);
        }
    }
    stars::LDeltaFunction::setNumPieces(property("dp_pieces", 8U));
    stars::ZAFunction::setNumPieces(property("fsp_pieces", 10U));
    stars::ZAFunction::setReductionQuality(property("fsp_reduction_quality", 10U));
    MMPDispatcher::setBeta(property("mmp_beta", 0.5));
    FSPDispatcher::setBeta(property("fsp_beta", 2.0));
    FSPDispatcher::discard = property("fsp_discard", false);
    FSPDispatcher::discardRatio = property("fsp_discard_ratio", 2.0);
    stars::FSPTaskList::setPreemptive(property("fsp_preemptive", true));
    SimAppDatabase::reset();
    StarsNode::Configuration::getInstance().setup(property);
}


void StarsNode::setup(unsigned int addr) {
    localAddress = CommAddress(addr, ConfigurationManager::getInstance().getPort());
    Configuration & cfg = Configuration::getInstance();
    // Execution power follows discretized pareto distribution, with k=1
    power = cfg.cpuVar();
    mem = cfg.memVar();
    disk = cfg.diskVar();

    createServices();
    // Load service state if needed
    if (cfg.inFile.is_open()) {
        unpackState(cfg.in);
    }
}


void StarsNode::receiveMessage(uint32_t src, boost::shared_ptr<BasicMsg> msg) {
    // Check if it is the timer
    if (msg == timerMsg) {
        Time ct = Time::getCurrentTime();
        while (!timerList.empty()) {
            if (timerList.front().timeout <= ct) {
                if (timerList.front().timeout < ct)
                    Logger::msg("Sim.Progress", WARN, "Timer arriving ", (ct - timerList.front().timeout).seconds(), " seconds late: ",
                            *timerList.front().msg);
                Simulator::getInstance().sendMessage(localAddress.getIPNum(), localAddress.getIPNum(), timerList.front().msg);
                timerList.pop_front();
            } else {
                if (!(timerList.front().id & 1)) {
                    // Program next timer
                    Simulator::getInstance().injectMessage(localAddress.getIPNum(), localAddress.getIPNum(), timerMsg,
                            timerList.front().timeout - ct);
                    ++timerList.front().id;
                }
                break;
            }
        }
    } else {
        enqueueMessage(CommAddress(src, ConfigurationManager::getInstance().getPort()), msg);
        processNextMessage();
    }
}


void StarsNode::finish() {
    Configuration & cfg = Configuration::getInstance();
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


void StarsNode::fail() {
    Logger::msg("Sim.Fail", DEBUG, "Fails node ", localAddress);

    // Stop running task at failed node
    if (!getSch().getTasks().empty())
        getSch().getTasks().front()->abort();
    // Remove timers not belonging to SubmissionNode
    for (list<Timer>::iterator i = timerList.begin(); i != timerList.end();)
        if (i->msg->getName() != "HeartbeatTimeout" && i->msg->getName() != "RequestTimeout")
            i = timerList.erase(i);
        else ++i;
    // Reset submission node, scheduler and dispatcher
    delete services[Disp];
    delete services[Sch];
    switch (Configuration::getInstance().getPolicy()) {
        case Configuration::MMPolicy:
            services[Sch] = new MMPScheduler(getLeaf());
            services[Disp] = new MMPDispatcher(getBranch());
            break;
        case Configuration::DPolicy:
            services[Sch] = new DPScheduler(getLeaf());
            services[Disp] = new DPDispatcher(getBranch());
            break;
        case Configuration::FSPolicy:
            services[Sch] = new stars::FSPScheduler(getLeaf());
            services[Disp] = new FSPDispatcher(getBranch());
            break;
        default:
            services[Sch] = new IBPScheduler(getLeaf());
            services[Disp] = new IBPDispatcher(getBranch());
            break;
    }
}


void StarsNode::createServices() {
    services.push_back(new SimOverlayBranch());
    services.push_back(new SimOverlayLeaf());
    services.push_back(new SubmissionNode(getLeaf()));
    switch (Configuration::getInstance().getPolicy()) {
        case Configuration::MMPolicy:
            services.push_back(new MMPScheduler(getLeaf()));
            services.push_back(new MMPDispatcher(getBranch()));
            break;
        case Configuration::DPolicy:
            services.push_back(new DPScheduler(getLeaf()));
            services.push_back(new DPDispatcher(getBranch()));
            break;
        case Configuration::FSPolicy:
            services.push_back(new stars::FSPScheduler(getLeaf()));
            services.push_back(new FSPDispatcher(getBranch()));
            break;
        default:
            services.push_back(new IBPScheduler(getLeaf()));
            services.push_back(new IBPDispatcher(getBranch()));
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
    ar & power & mem & disk & static_cast<SimOverlayBranch &>(getBranch()) & static_cast<SimOverlayLeaf &>(getLeaf());
    switch (Configuration::getInstance().getPolicy()) {
        case Configuration::IBPolicy:
            static_cast<IBPDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case Configuration::MMPolicy:
            static_cast<MMPDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case Configuration::DPolicy:
            static_cast<DPDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case Configuration::FSPolicy:
            static_cast<FSPDispatcher &>(*services[Disp]).serializeState(ar);
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
    ar & power & mem & disk & static_cast<SimOverlayBranch &>(getBranch()) & static_cast<SimOverlayLeaf &>(getLeaf());
    switch (Configuration::getInstance().getPolicy()) {
        case Configuration::IBPolicy:
            static_cast<IBPDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case Configuration::MMPolicy:
            static_cast<MMPDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case Configuration::DPolicy:
            static_cast<DPDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        case Configuration::FSPolicy:
            static_cast<FSPDispatcher &>(*services[Disp]).serializeState(ar);
            break;
        default:
            break;
    }
}


unsigned int StarsNode::getBranchLevel() const {
    if (!getBranch().inNetwork())
        return Simulator::getInstance().getNode(getLeaf().getFatherAddress().getIPNum()).getBranchLevel() + 1;
    else if (getBranch().getFatherAddress() != CommAddress())
        return Simulator::getInstance().getNode(getBranch().getFatherAddress().getIPNum()).getBranchLevel() + 1;
    else return 0;
}


//void StarsNode::showPartialTree(bool isBranch, log4cpp::Priority::Value prio) {
//    if (log4cpp::Category::getInstance("Sim.Tree").isPriorityEnabled(prio)) {
//        SimOverlayBranch * father = NULL;
//        uint32_t fatherIP = 0;
//        if (isBranch) {
//            if (getBranch().getFatherAddress() != CommAddress()) {
//                fatherIP = getBranch().getFatherAddress().getIPNum();
//                father = &static_cast<SimOverlayBranch &>(Simulator::getInstance().getNode(fatherIP).getBranch());
//            }
//        }
//        else {
//            if (getLeaf().getFatherAddress() != CommAddress()) {
//                fatherIP = getBranch().getFatherAddress().getIPNum();
//                father = &static_cast<SimOverlayBranch &>(Simulator::getInstance().getNode(fatherIP).getBranch());
//            } else {
//                // This may be an error...
//                Logger::msg("Sim.Tree", WARN, "Leaf node without father???: L@", localAddress);
//                return;
//            }
//        }
//
//        if (father != NULL) {
//            Logger::msg("Sim.Tree", prio, "B@", Simulator::getInstance().getNode(fatherIP).getLocalAddress(), ": ", *father);
//            // Right child
//            Logger::msg("Sim.Tree", prio, "  |- ", father->getRightZone();
//            CommAddress childAddr = father->getRightAddress();
//            StarsNode & rightChild = Simulator::getInstance().getNode(childAddr.getIPNum());
//            if (!father->isRightLeaf()) {
//                rightChild.showRecursive(prio, childAddr == localAddress ? 1 : 0, "  |  ");
//            } else {
//                Logger::msg("Sim.Tree", prio, "  |  ", "L@", rightChild.localAddress, ": "
//                        << static_cast<SimOverlayLeaf &>(rightChild.getLeaf());
//            }
//
//            // Left child
//            Logger::msg("Sim.Tree", prio, "  \\- ", father->getLeftZone();
//            childAddr = father->getLeftAddress();
//            StarsNode & leftChild = Simulator::getInstance().getNode(childAddr.getIPNum());
//            if (!father->isLeftLeaf()) {
//                leftChild.showRecursive(prio, childAddr == localAddress ? 1 : 0, "     ");
//            } else {
//                Logger::msg("Sim.Tree", prio, "     ", "L@", leftChild.localAddress, ": "
//                        << static_cast<SimOverlayLeaf &>(leftChild.getLeaf());
//            }
//        }
//        else showRecursive(prio, 1);
//    }
//}


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


void StarsNode::buildDispatcher() {
    // Generate Dispatcher state
    switch (Configuration::getInstance().getPolicy()) {
    case Configuration::IBPolicy:
        buildDispatcherGen<IBPDispatcher>();
        break;
    case Configuration::MMPolicy:
        buildDispatcherGen<MMPDispatcher>();
        break;
    case Configuration::DPolicy:
        buildDispatcherGen<DPDispatcher>();
        break;
    case Configuration::FSPolicy:
        buildDispatcherGen<FSPDispatcher>();
        break;
    default:
        break;
    }
}


template <class T> void StarsNode::buildDispatcherGen() {
    Simulator & sim = Simulator::getInstance();
    vector<void *> vv(200);
    MemoryOutArchive oaa(vv.begin());
    typename T::Link fatherLink, leftLink, rightLink;
    fatherLink.addr = getBranch().getFatherAddress();
    leftLink.addr = getBranch().getChildAddress(0);
    rightLink.addr = getBranch().getChildAddress(1);
    if (getBranch().isLeaf(0))
        leftLink.availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(leftLink.addr.getIPNum()).getSch().getAvailability().clone()));
    else
        leftLink.availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(leftLink.addr.getIPNum()).getDisp().getBranchInfo()->clone()));
    if (getBranch().isLeaf(1))
        rightLink.availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(rightLink.addr.getIPNum()).getSch().getAvailability().clone()));
    else
        rightLink.availInfo.reset(static_cast<typename T::availInfoType *>(sim.getNode(rightLink.addr.getIPNum()).getDisp().getBranchInfo()->clone()));
    leftLink.availInfo->reduce();
    rightLink.availInfo->reduce();
    fatherLink.serializeState(oaa);
    leftLink.serializeState(oaa);
    rightLink.serializeState(oaa);
    MemoryInArchive iaa(vv.begin());
    static_cast<T &>(getDisp()).serializeState(iaa);
    static_cast<T &>(getDisp()).recomputeInfo();
}


void StarsNode::buildDispatcherDown() {
    // Generate Dispatcher state
    switch (Configuration::getInstance().getPolicy()) {
    case Configuration::MMPolicy:
        buildDispatcherDownGen<MMPDispatcher>();
        break;
    case Configuration::FSPolicy:
        buildDispatcherDownGen<FSPDispatcher>();
        break;
    default:
        break;
    }
}


template <class T> void StarsNode::buildDispatcherDownGen() {
    Simulator & sim = Simulator::getInstance();
    vector<void *> vv(200);
    MemoryOutArchive oaa(vv.begin());
    static_cast<T &>(getDisp()).serializeState(oaa);
    CommAddress fatherAddr = *static_cast<CommAddress *>(vv[0]);
    boost::shared_ptr<typename T::availInfoType> & fatherInfo = *static_cast<boost::shared_ptr<typename T::availInfoType> *>(vv[1]);
    if (fatherAddr != CommAddress()) {
        StarsNode & father = sim.getNode(fatherAddr.getIPNum());
        if (father.getBranch().getChildAddress(0) == localAddress) {
            fatherInfo.reset(static_cast<T &>(father.getDisp()).getChildWaitingInfo(0)->clone());
        } else {
            fatherInfo.reset(static_cast<T &>(father.getDisp()).getChildWaitingInfo(1)->clone());
        }
    }
    static_cast<T &>(getDisp()).recomputeInfo();
}

