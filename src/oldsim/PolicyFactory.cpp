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

#include <vector>
#include "PolicyFactory.hpp"
#include "DPScheduler.hpp"
#include "DPDispatcher.hpp"
#include "MMPScheduler.hpp"
#include "MMPDispatcher.hpp"
#include "IBPScheduler.hpp"
#include "IBPDispatcher.hpp"
#include "FSPScheduler.hpp"
#include "FSPDispatcher.hpp"
#include "Simulator.hpp"
#include "SlaveLocalScheduler.hpp"
#include "CentralizedScheduler.hpp"
using std::vector;

struct IBP {
    typedef IBPScheduler scheduler;
    typedef IBPDispatcher dispatcher;
    typedef IBPAvailabilityInformation information;
};

struct MMP {
    typedef MMPScheduler scheduler;
    typedef MMPDispatcher dispatcher;
    typedef MMPAvailabilityInformation information;
};

struct DP {
    typedef DPScheduler scheduler;
    typedef DPDispatcher dispatcher;
    typedef DPAvailabilityInformation information;
};

struct FSP {
    typedef stars::FSPScheduler scheduler;
    typedef FSPDispatcher dispatcher;
    typedef FSPAvailabilityInformation information;
};

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


template <class Policy> class PolicyFactoryDist : public PolicyFactory {
    virtual Service * createScheduler(OverlayLeaf & leaf) const { return new typename Policy::scheduler(leaf); }
    virtual Service * createDispatcher(OverlayBranch & branch) const { return new typename Policy::dispatcher(branch); }
    virtual void serializeDispatcher(Service & disp, MsgpackOutArchive & ar) const {
        static_cast<typename Policy::dispatcher &>(disp).serializeState(ar);
    }
    virtual void serializeDispatcher(Service & disp, MsgpackInArchive & ar) const {
        static_cast<typename Policy::dispatcher &>(disp).serializeState(ar);
    }
    virtual boost::shared_ptr<CentralizedScheduler> getCentScheduler() const {
        return boost::shared_ptr<CentralizedScheduler>();
    }
    virtual void buildDispatcher(OverlayBranch & branch, Service & disp) const {
        Simulator & sim = Simulator::getInstance();
        vector<void *> vv(200);
        MemoryOutArchive oaa(vv.begin());
        typename Policy::dispatcher::Link fatherLink, leftLink, rightLink;
        fatherLink.addr = branch.getFatherAddress();
        leftLink.addr = branch.getChildAddress(0);
        rightLink.addr = branch.getChildAddress(1);
        if (branch.isLeaf(0))
            leftLink.availInfo.reset(static_cast<typename Policy::information *>(sim.getNode(leftLink.addr.getIPNum()).getSch().getAvailability()->clone()));
        else
            leftLink.availInfo.reset(static_cast<typename Policy::information *>(sim.getNode(leftLink.addr.getIPNum()).getDisp().getBranchInfo()->clone()));
        if (branch.isLeaf(1))
            rightLink.availInfo.reset(static_cast<typename Policy::information *>(sim.getNode(rightLink.addr.getIPNum()).getSch().getAvailability()->clone()));
        else
            rightLink.availInfo.reset(static_cast<typename Policy::information *>(sim.getNode(rightLink.addr.getIPNum()).getDisp().getBranchInfo()->clone()));
        leftLink.availInfo->reduce();
        rightLink.availInfo->reduce();
        fatherLink.serializeState(oaa);
        leftLink.serializeState(oaa);
        rightLink.serializeState(oaa);
        MemoryInArchive iaa(vv.begin());
        static_cast<typename Policy::dispatcher &>(disp).serializeState(iaa);
        static_cast<typename Policy::dispatcher &>(disp).recomputeInfo();
    }
    virtual void buildDispatcherDown(Service & disp, CommAddress localAddress) const {
        Simulator & sim = Simulator::getInstance();
        vector<void *> vv(200);
        MemoryOutArchive oaa(vv.begin());
        static_cast<typename Policy::dispatcher &>(disp).serializeState(oaa);
        CommAddress fatherAddr = *static_cast<CommAddress *>(vv[0]);
        boost::shared_ptr<typename Policy::information> & fatherInfo = *static_cast<boost::shared_ptr<typename Policy::information> *>(vv[1]);
        if (fatherAddr != CommAddress()) {
            StarsNode & father = sim.getNode(fatherAddr.getIPNum());
            if (father.getBranch().getChildAddress(0) == localAddress) {
                fatherInfo.reset(static_cast<typename Policy::dispatcher &>(father.getDisp()).getChildWaitingInfo(0)->clone());
            } else {
                fatherInfo.reset(static_cast<typename Policy::dispatcher &>(father.getDisp()).getChildWaitingInfo(1)->clone());
            }
        }
        static_cast<typename Policy::dispatcher &>(disp).recomputeInfo();
    }
};

template <> void PolicyFactoryDist<IBP>::buildDispatcherDown(Service & disp, CommAddress localAddress) const {}
template <> void PolicyFactoryDist<DP>::buildDispatcherDown(Service & disp, CommAddress localAddress) const {}


class DummyDispatcher : public DispatcherInterface {
    virtual boost::shared_ptr<AvailabilityInformation> getBranchInfo() const {
        return boost::shared_ptr<AvailabilityInformation>();
    }
    virtual boost::shared_ptr<AvailabilityInformation> getChildInfo(int child) const {
        return boost::shared_ptr<AvailabilityInformation>();
    }
    virtual bool receiveMessage(const CommAddress & src, const BasicMsg & msg) { return false; }
};


class PolicyFactoryCent : public PolicyFactory {
    virtual Service * createScheduler(OverlayLeaf & leaf) const { return new stars::SlaveLocalScheduler(leaf); }
    virtual Service * createDispatcher(OverlayBranch & branch) const { return new DummyDispatcher; }
    virtual boost::shared_ptr<CentralizedScheduler> getCentScheduler() const {
        return CentralizedScheduler::createScheduler(csType);
    }

    virtual void serializeDispatcher(Service & disp, MsgpackOutArchive & ar) const {}
    virtual void serializeDispatcher(Service & disp, MsgpackInArchive & ar) const {}
    virtual void buildDispatcher(OverlayBranch & branch, Service & disp) const {}
    virtual void buildDispatcherDown(Service & disp, CommAddress localAddress) const {}

    std::string csType;

public:
    PolicyFactoryCent(const std::string & type) : csType(type) {
        ConfigurationManager::getInstance().setHeartbeat(-1);
    }
};


template <class Policy> class PolicyFactoryBlind : public PolicyFactory {
    virtual Service * createScheduler(OverlayLeaf & leaf) const { return new typename Policy::scheduler(leaf); }
    virtual Service * createDispatcher(OverlayBranch & branch) const { return new DummyDispatcher; }
    virtual boost::shared_ptr<CentralizedScheduler> getCentScheduler() const {
        return CentralizedScheduler::createScheduler("blind");
    }

    virtual void serializeDispatcher(Service & disp, MsgpackOutArchive & ar) const {}
    virtual void serializeDispatcher(Service & disp, MsgpackInArchive & ar) const {}
    virtual void buildDispatcher(OverlayBranch & branch, Service & disp) const {}
    virtual void buildDispatcherDown(Service & disp, CommAddress localAddress) const {}

public:
    PolicyFactoryBlind() {
        ConfigurationManager::getInstance().setHeartbeat(-1);
    }
};


std::unique_ptr<PolicyFactory> PolicyFactory::getFactory(const std::string & scheduler, const std::string & policy) {
    if (scheduler == "dist") {
        if (policy == "IBP")
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryDist<IBP>);
        else if (policy == "MMP")
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryDist<MMP>);
        else if (policy == "DP" )
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryDist<DP>);
        else if (policy == "FSP")
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryDist<FSP>);
    } else if (scheduler == "cent") {
        return std::unique_ptr<PolicyFactory>(new PolicyFactoryCent(policy));
    } else if (scheduler == "blind") {
        if (policy == "IBP")
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryBlind<IBP>);
        else if (policy == "MMP")
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryBlind<MMP>);
        else if (policy == "DP" )
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryBlind<DP>);
        else if (policy == "FSP")
            return std::unique_ptr<PolicyFactory>(new PolicyFactoryBlind<FSP>);
    }
    return std::unique_ptr<PolicyFactory>(new PolicyFactoryCent("blind"));
}
