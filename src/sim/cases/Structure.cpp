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

#include <algorithm>
#include <vector>
#include <list>
#include "../Simulator.hpp"
#include "../SimulationCase.hpp"
#include "StructureNode.hpp"
#include "ResourceNode.hpp"
#include "IBPDispatcher.hpp"
#include "MMPDispatcher.hpp"
#include "DPDispatcher.hpp"
#include "MSPDispatcher.hpp"


class createTree : public SimulationCase {
public:
    createTree(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const std::string getName() { return std::string("createTree"); }

    void preStart() {
        // NOTE: outside MSG_main, do not call Simulator::getCurrentNode() !!
        Simulator & sim = Simulator::getInstance();
        uint32_t numNodes = sim.getNumNodes();
        uint32_t p2NumNodes = 1;
        for (uint32_t i = numNodes; i > 1; i >>= 1) p2NumNodes <<= 1;

        std::vector<uint32_t> treeNodes(numNodes);
        for (uint32_t i = 0; i < numNodes; i++) treeNodes[i] = i;
        // Shuffle
        std::random_shuffle(treeNodes.begin(), treeNodes.end());

        std::list<snode> toCreate;

        if (3 * p2NumNodes / 2 >= numNodes) {
            // Ok enough nodes for 3 children in level 0
            uint32_t threeChildren = numNodes - p2NumNodes;
            // Generate ResourceNodes
            for (uint32_t spos = 0, rpos = 0; spos < p2NumNodes / 2; spos++) {
                int numChildren = spos < threeChildren ? 3 : 2;
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                n.level = 0;
                for (int i = 0; i < numChildren; rpos++, i++) {
                    generateRNode(sim.getNode(rpos), n.addr);
                    n.children.push_back(rpos);
                }
                toCreate.push_back(n);
            }
            // Generate structure nodes
            while (toCreate.size() > 1) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                for (uint32_t i = 0; i < 2; i++) {
                    snode c = toCreate.front();
                    toCreate.pop_front();
                    if (c.children.size() == 2)
                        generateSNode(sim.getNode(c.addr), n.addr, c.children[0], c.children[1], c.level);
                    else
                        generateSNode(sim.getNode(c.addr), n.addr, c.children[0], c.children[1], c.children[2], c.level);
                    n.children.push_back(c.addr);
                    n.level = c.level + 1;
                }
                toCreate.push_back(n);
            }
            // Root node
            snode root = toCreate.front();
            generateSNode(sim.getNode(root.addr), root.addr, root.children[0], root.children[1], root.level);
        } else {
            // We also need some level 1 nodes with 3 children
            uint32_t twoChildren = 0;
            if (numNodes % 3 == 1) twoChildren = 2;
            else if (numNodes % 3 == 2) twoChildren = 1;
            uint32_t threeChildren = (numNodes - twoChildren * 2) / 3;
            uint32_t level1threeChildren = threeChildren + twoChildren - p2NumNodes / 2;
            // Generate ResourceNodes
            for (uint32_t spos = 0, rpos = 0; spos < threeChildren + twoChildren; spos++) {
                int numChildren = spos < threeChildren ? 3 : 2;
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                n.level = 0;
                for (int i = 0; i < numChildren; rpos++, i++) {
                    generateRNode(sim.getNode(rpos), n.addr);
                    n.children.push_back(rpos);
                }
                toCreate.push_back(n);
            }
            // Generate structure nodes
            for (uint32_t j = 0; toCreate.size() > 1; j++) {
                int numChildren = j < level1threeChildren ? 3 : 2;
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                for (uint32_t i = 0; i < numChildren; i++) {
                    snode c = toCreate.front();
                    toCreate.pop_front();
                    if (c.children.size() == 2)
                        generateSNode(sim.getNode(c.addr), n.addr, c.children[0], c.children[1], c.level);
                    else
                        generateSNode(sim.getNode(c.addr), n.addr, c.children[0], c.children[1], c.children[2], c.level);
                    n.children.push_back(c.addr);
                    n.level = c.level + 1;
                }
                toCreate.push_back(n);
            }
            // Root node
            snode root = toCreate.front();
            generateSNode(sim.getNode(root.addr), root.addr, root.children[0], root.children[1], root.level);
        }

        // Prevent the simulator from running
        sim.stop();
    }

private:
    void generateRNode(StarsNode & node, uint32_t rfather) {
        std::vector<void *> v(10);
        MemoryOutArchive oa(v.begin());
        // Generate ResourceNode information
        boost::shared_ptr<ZoneDescription> rzone(new ZoneDescription);
        CommAddress father(rfather, ConfigurationManager::getInstance().getPort());
        rzone->setMinAddress(father);
        rzone->setMaxAddress(father);
        uint64_t seq = 0;
        oa << father << seq << rzone << rzone;
        MemoryInArchive ia(v.begin());
        node.getResourceNode().serializeState(ia);
    }

    void generateSNode(StarsNode & node, uint32_t sfather, uint32_t schild1, uint32_t schild2, int level) {
        Simulator & sim = Simulator::getInstance();
        std::vector<void *> v(10);
        MemoryOutArchive oa(v.begin());
        // Generate StructureNode information
        boost::shared_ptr<ZoneDescription> zone(new ZoneDescription);
        CommAddress father;
        if (sfather != node.getLocalAddress().getIPNum())
            father = CommAddress(sfather, ConfigurationManager::getInstance().getPort());
        std::list<boost::shared_ptr<TransactionalZoneDescription> > subZones;
        boost::shared_ptr<ZoneDescription> zone1, zone2;
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
            zone1.reset(new ZoneDescription(*sim.getNode(schild1).getSructureNode().getZoneDesc()));
            zone2.reset(new ZoneDescription(*sim.getNode(schild2).getSructureNode().getZoneDesc()));
            zone->setMinAddress(zone1->getMinAddress());
            zone->setMaxAddress(zone2->getMaxAddress());
        }
        subZones.push_back(boost::shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
        subZones.push_back(boost::shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
        subZones.front()->setLink(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        subZones.front()->setZone(zone1);
        subZones.front()->commit();
        subZones.back()->setLink(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
        subZones.back()->setZone(zone2);
        subZones.back()->commit();
        uint64_t seq = 0;
        unsigned int m = 2;
        int state = StructureNode::ONLINE;
        oa << state << m << level << zone << zone << father << seq << subZones;
        MemoryInArchive ia(v.begin());
        node.getSructureNode().serializeState(ia);

        // Generate Dispatcher state
        switch (node.getPolicyType()) {
        case StarsNode::SimpleSchedulerClass:
            generateDispatcher<IBPDispatcher>(node, father, schild1, schild2, level);
            break;
        case StarsNode::FCFSSchedulerClass:
            generateDispatcher<MMPDispatcher>(node, father, schild1, schild2, level);
            break;
        case StarsNode::EDFSchedulerClass:
            generateDispatcher<DPDispatcher>(node, father, schild1, schild2, level);
            break;
        case StarsNode::MinSlownessSchedulerClass:
            generateDispatcher<MSPDispatcher>(node, father, schild1, schild2, level);
            break;
        default:
            break;
        }
    }


    template <class T>
    void generateDispatcher(StarsNode & node, const CommAddress & father, uint32_t schild1, uint32_t schild2, int level) {
        Simulator & sim = Simulator::getInstance();
        std::vector<void *> vv(10);
        MemoryOutArchive oaa(vv.begin());
        std::vector<typename T::Link> children(2);
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
        oaa << fatherLink << children;
        MemoryInArchive iaa(vv.begin());
        static_cast<T &>(node.getDispatcher()).serializeState(iaa);
    }


    void generateSNode(StarsNode & node, uint32_t sfather, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level) {
        Simulator & sim = Simulator::getInstance();
        std::vector<void *> v(10);
        MemoryOutArchive oa(v.begin());
        // Generate StructureNode information
        boost::shared_ptr<ZoneDescription> zone(new ZoneDescription);
        CommAddress father;
        if (sfather != node.getLocalAddress().getIPNum())
            father = CommAddress(sfather, ConfigurationManager::getInstance().getPort());
        std::list<boost::shared_ptr<TransactionalZoneDescription> > subZones;
        boost::shared_ptr<ZoneDescription> zone1, zone2, zone3;
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
            zone1.reset(new ZoneDescription(*sim.getNode(schild1).getSructureNode().getZoneDesc()));
            zone2.reset(new ZoneDescription(*sim.getNode(schild2).getSructureNode().getZoneDesc()));
            zone3.reset(new ZoneDescription(*sim.getNode(schild3).getSructureNode().getZoneDesc()));
            zone->setMinAddress(zone1->getMinAddress());
            zone->setMaxAddress(zone3->getMaxAddress());
        }
        subZones.push_back(boost::shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
        subZones.back()->setLink(CommAddress(schild1, ConfigurationManager::getInstance().getPort()));
        subZones.back()->setZone(zone1);
        subZones.back()->commit();
        subZones.push_back(boost::shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
        subZones.back()->setLink(CommAddress(schild2, ConfigurationManager::getInstance().getPort()));
        subZones.back()->setZone(zone2);
        subZones.back()->commit();
        subZones.push_back(boost::shared_ptr<TransactionalZoneDescription>(new TransactionalZoneDescription));
        subZones.back()->setLink(CommAddress(schild3, ConfigurationManager::getInstance().getPort()));
        subZones.back()->setZone(zone3);
        subZones.back()->commit();
        unsigned int m = 2;
        uint64_t seq = 0;
        int state = StructureNode::ONLINE;
        oa << state << m << level << zone << zone << father << seq << subZones;
        MemoryInArchive ia(v.begin());
        node.getSructureNode().serializeState(ia);

        // Generate Dispatcher state
        switch (node.getPolicyType()) {
        case StarsNode::SimpleSchedulerClass:
            generateDispatcher<IBPDispatcher>(node, father, schild1, schild2, schild3, level);
            break;
        case StarsNode::FCFSSchedulerClass:
            generateDispatcher<MMPDispatcher>(node, father, schild1, schild2, schild3, level);
            break;
        case StarsNode::EDFSchedulerClass:
            generateDispatcher<DPDispatcher>(node, father, schild1, schild2, schild3, level);
            break;
        case StarsNode::MinSlownessSchedulerClass:
            generateDispatcher<MSPDispatcher>(node, father, schild1, schild2, schild3, level);
            break;
        default:
            break;
        }
    }

    template <class T>
    void generateDispatcher(StarsNode & node, const CommAddress & father, uint32_t schild1, uint32_t schild2, uint32_t schild3, int level) {
        Simulator & sim = Simulator::getInstance();
        std::vector<void *> vv(10);
        MemoryOutArchive oaa(vv.begin());
        std::vector<typename T::Link> children(3);
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
        oaa << fatherLink << children;
        MemoryInArchive iaa(vv.begin());
        static_cast<T &>(node.getDispatcher()).serializeState(iaa);
    }

    struct snode {
        std::vector<uint32_t> children;
        uint32_t addr;
        int level;
        snode() { children.reserve(3); }
    };

    class MemoryInArchive {
        std::vector<void *>::iterator ptr;
    public:
        typedef boost::mpl::bool_<true> is_loading;
        MemoryInArchive(std::vector<void *>::iterator o) : ptr(o) {}
        template<class T> MemoryInArchive & operator>>(T & o) {
            o = *static_cast<T *>(*ptr++);
            return *this;
        }
        template<class T> MemoryInArchive & operator&(T & o) {
            return operator>>(o);
        }
    };

    class MemoryOutArchive {
        std::vector<void *>::iterator ptr;
    public:
        typedef boost::mpl::bool_<false> is_loading;
        MemoryOutArchive(std::vector<void *>::iterator o) : ptr(o) {}
        template<class T> MemoryOutArchive & operator<<(T & o) {
            *ptr++ = &o;
            return *this;
        }
        template<class T> MemoryOutArchive & operator&(T & o) {
            return operator<<(o);
        }
    };
};
REGISTER_SIMULATION_CASE(createTree);
