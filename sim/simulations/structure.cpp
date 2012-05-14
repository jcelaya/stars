/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
 *
 *  This file is part of PeerComp.
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

#include "../Simulator.hpp"
#include "../SimulationCase.hpp"
#include "StructureNode.hpp"
#include "TaskDescription.hpp"
#include "../TrafficStatistics.hpp"
#include "InsertCommandMsg.hpp"
#include "Logger.hpp"
using namespace std;
using namespace boost::posix_time;



/// Test cases

// Copy-Paste to create new test hehehe
//class SimulatorTesting : public PeerCompSimulation<SimulatorTesting> {
//public:
// SimulatorTesting(const Properties & p) : PeerCompSimulation<SimulatorTesting>(p) {
//  // Prepare the properties
// }
//
// static const string getName() { return string("SimulatorTesting"); }
//
// void preStart() {
//  // Before running simulation
// }
//
// void postEnd() {
//  // After ending simulation
// }
//};
//REGISTER_SIMULATION_CASE(SimulatorTesting);


///// Scenario 1: Create network
//class networkCreation : public PeerCompSimulation<networkCreation> {
//public:
// networkCreation(const Properties & p) : PeerCompSimulation<networkCreation>(p) {
//  // Prepare the properties
//  property["num_nodes"] = "1";
//  property["fanout"] = "2";
// }
//
// static const string getName() { return string("networkCreation"); }
//
// void preStart() {
//  InsertCommandMsg icm;
//  icm.setWhere(CommAddress(0));
//  sim.sendMessage(0, 0, icm.clone());
// }
//
// void postEnd() {
//  sim.showStatistics();
//  PeerCompNode::showTree(log4cpp::Priority::INFO);
// }
//};
//REGISTER_SIMULATION_CASE(networkCreation);
//
//
///// Scenario 2: Add execution nodes to the network.
//class oneInsertion : public PeerCompSimulation<oneInsertion> {
//public:
// oneInsertion(const Properties & p) : PeerCompSimulation<oneInsertion>(p) {
//  // Prepare the properties
//  property["num_nodes"] = "2";
//  property["fanout"] = "2";
// }
//
// static const string getName() { return string("oneInsertion"); }
//
// void preStart() {
//  InsertCommandMsg icm;
//  icm.setWhere(CommAddress(0));
//  sim.sendMessage(0, 0, icm.clone());
//  sim.sendMessage(1, 1, icm.clone());
// }
//
// void postEnd() {
//  sim.showStatistics();
//  PeerCompNode::showTree(log4cpp::Priority::INFO);
// }
//};
//REGISTER_SIMULATION_CASE(oneInsertion);
//
//
///// Scenario 3: Add execution nodes to the network until the root splits.
//class firstRootSplit : public PeerCompSimulation<firstRootSplit> {
//public:
// firstRootSplit(const Properties & p) : PeerCompSimulation<firstRootSplit>(p) {
//  // Prepare the properties
//  property["num_nodes"] = "4";
//  property["fanout"] = "2";
// }
//
// static const string getName() { return string("firstRootSplit"); }
//
// void preStart() {
//  // Before running simulation
//  InsertCommandMsg icm;
//  icm.setWhere(CommAddress(0));
//  sim.sendMessage(0, 0, icm.clone());
//  sim.sendMessage(1, 1, icm.clone());
//  sim.sendMessage(2, 2, icm.clone());
//  sim.sendMessage(3, 3, icm.clone());
// }
//
// void postEnd() {
//  // After ending simulation
//  sim.showStatistics();
//  PeerCompNode::showTree(log4cpp::Priority::INFO);
// }
//};
//REGISTER_SIMULATION_CASE(firstRootSplit);
//
//
///// Scenario 4: Add a lot of execution nodes to the network
//class sequentialNodeInsertion : public PeerCompSimulation<sequentialNodeInsertion> {
// unsigned int nextInsert;
// unsigned int numNodes;
// InsertCommandMsg icm;
//
//public:
// sequentialNodeInsertion(const Properties & p) : PeerCompSimulation<sequentialNodeInsertion>(p) {
//  // Prepare the properties
// }
//
// static const string getName() { return string("sequentialNodeInsertion"); }
//
// void preStart() {
//  // Before running simulation
//  numNodes = sim.getNumNodes();
//  nextInsert = 1;
//  sim.registerHandler(*this);
//
//  icm.setWhere(CommAddress(0));
//  LogMsg("Sim.St"(), INFO) << "Inserting node E0";
//  sim.sendMessage(0, 0, icm.clone());
// }
//
// void afterEvent(const Simulator::Event & ev) {
//  if (sim.emptyEventQueue() && nextInsert < numNodes) {
//   LogMsg("Sim.St"(), INFO) << "";
//   LogMsg("Sim.St"(), INFO) << "";
//   LogMsg("Sim.St"(), INFO) << "Inserting node E" << nextInsert;
//   PeerCompNode::showTree();
//   sim.sendMessage(nextInsert, nextInsert, icm.clone());
//   nextInsert++;
//  }
// }
//
// void postEnd() {
//  LogMsg("Sim.St"(), INFO) << "";
//  LogMsg("Sim.St"(), INFO) << "";
//  // After ending simulation
//  sim.showStatistics();
//  PeerCompNode::showTree(log4cpp::Priority::INFO);
//  PeerCompNode::checkTree();
//  PeerCompNode::saveState();
// }
//};
//REGISTER_SIMULATION_CASE(sequentialNodeInsertion);
//
//
///// Scenario 6: Add a lot of execution nodes to the network in a random order
//class randomNodeInsertion : public PeerCompSimulation<randomNodeInsertion> {
// int * ee;
// unsigned int numNodes;
// unsigned int nextInsert;
// InsertCommandMsg icm;
//
//public:
// randomNodeInsertion(const Properties & p) : PeerCompSimulation<randomNodeInsertion>(p) {
//  // Prepare the properties
// }
//
// static const string getName() { return string("randomNodeInsertion"); }
//
// void preStart() {
//  // Before running simulation
//  numNodes = sim.getNumNodes();
//  nextInsert = 1;
//  sim.registerHandler(*this);
//
//  ee = new int[numNodes];
//  for (unsigned int i = 0; i < numNodes; i++) ee[i] = i;
//
//  // Shuffle it
//  for (unsigned int i = 1; i < numNodes - 1; i++) {
//   // random is a number between i + 1 and numNodes - 1
//   int random = Simulator::uniform((int)i + 1, (int)numNodes - 1);
//   int tmp = ee[random];
//   ee[random] = ee[i];
//   ee[i] = tmp;
//  }
//  // The first one is always 0 ;P
//
//  icm.setWhere(CommAddress(0));
//
//  LogMsg("Sim.St"(), DEBUG) << "Inserting node E0";
//  sim.sendMessage(0, 0, icm.clone());
// }
//
// void afterEvent(const Simulator::Event & ev) {
//  if (sim.emptyEventQueue() && nextInsert < numNodes) {
//   LogMsg("Sim.St"(), INFO) << "";
//   LogMsg("Sim.St"(), INFO) << "";
//   int addr = ee[nextInsert];
//   LogMsg("Sim.St"(), DEBUG) << "Inserting node E" << addr;
//   sim.sendMessage(addr, addr, icm.clone());
//   nextInsert++;
//  }
// }
//
// void postEnd() {
//  LogMsg("Sim.St"(), INFO) << "";
//  LogMsg("Sim.St"(), INFO) << "";
//  // After ending simulation
//  delete[] ee;
//  sim.showStatistics();
//  PeerCompNode::showTree(log4cpp::Priority::INFO);
//  PeerCompNode::checkTree();
//  PeerCompNode::saveState();
// }
//};
//REGISTER_SIMULATION_CASE(randomNodeInsertion);
//
//
///// Scenario 7: Add a lot of execution nodes to the network in a concurrent way
//class concurrentNodeInsertion : public PeerCompSimulation<concurrentNodeInsertion> {
// InsertCommandMsg icm;
// unsigned int numNodes;
// bool firstInsert;
//public:
// concurrentNodeInsertion(const Properties & p) : PeerCompSimulation<concurrentNodeInsertion>(p) {
//  // Prepare the properties
// }
//
// static const string getName() { return string("concurrentNodeInsertion"); }
//
// void preStart() {
//  // Before running simulation
//  numNodes = sim.getNumNodes();
//  firstInsert = true;
//  sim.registerHandler(*this);
//
//  LogMsg("Sim.St"(), DEBUG) << "Inserting node E0";
//  icm.setWhere(CommAddress(0));
//  sim.sendMessage(0, 0, icm.clone());
// }
//
// void afterEvent(const Simulator::Event & ev) {
//  if (firstInsert && sim.emptyEventQueue()) {
//   LogMsg("Sim.St"(), DEBUG) << "";
//   LogMsg("Sim.St"(), DEBUG) << "";
//   LogMsg("Sim.St"(), DEBUG) << "Inserting the other nodes";
//   LogMsg("Sim.St"(), DEBUG) << "";
//   for (unsigned int i = 1; i < numNodes; i++ ) {
//    LogMsg("Sim.St"(), DEBUG) << "Inserting node E" << i;
//    sim.sendMessage(i, i, icm.clone());
//   }
//   firstInsert = false;
//  }
// }
//
// void postEnd() {
//  // After ending simulation
//  sim.showStatistics();
//  PeerCompNode::showTree(log4cpp::Priority::INFO);
//  PeerCompNode::checkTree();
//  PeerCompNode::saveState();
// }
//};
//REGISTER_SIMULATION_CASE(concurrentNodeInsertion);


/// Scenario 8: Check network
class networkCheck : public SimulationCase {
public:
    networkCheck(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const string getName() {
        return string("networkCheck");
    }

    void preStart() {
        // Prevent any timer from running the simulation
        Simulator::getInstance().stop();
    }
};
REGISTER_SIMULATION_CASE(networkCheck);


/// Scenario 9: Create random tree at zero
class createTree : public SimulationCase {
    struct snode {
        vector<uint32_t> children;
        uint32_t addr;
        int level;
        snode() {
            children.reserve(3);
        }
    };

public:
    createTree(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const string getName() {
        return string("createTree");
    }

    void preStart() {
        uint32_t numNodes = sim.getNumNodes();
        uint32_t p2NumNodes = 1;
        for (uint32_t i = numNodes; i > 1; i >>= 1) p2NumNodes <<= 1;

        vector<uint32_t> treeNodes(numNodes);
        for (uint32_t i = 0; i < numNodes; i++) treeNodes[i] = i;
        // Shuffle
        random_shuffle(treeNodes.begin(), treeNodes.end());

        list<snode> toCreate;

        if (3 * p2NumNodes / 2 >= numNodes) {
            // Ok enough nodes for 3 children in level 0
            uint32_t threeChildren = numNodes - p2NumNodes;
            // Generate ResourceNodes
            // Generate groups of 3
            unsigned int rpos = 0;
            unsigned int toCreateSize = 0;
            for (uint32_t spos = 0; spos < threeChildren; spos++) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                n.level = 0;
                for (uint32_t i = 0; i < 3; rpos++, i++) {
                    sim.getNode(rpos).generateRNode(n.addr);
                    n.children.push_back(rpos);
                }
                toCreate.push_back(n);
                toCreateSize++;
            }
            // Generate groups of 2
            for (uint32_t spos = 0; spos < p2NumNodes / 2 - threeChildren; spos++) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                n.level = 0;
                for (uint32_t i = 0; i < 2; rpos++, i++) {
                    sim.getNode(rpos).generateRNode(n.addr);
                    n.children.push_back(rpos);
                }
                toCreate.push_back(n);
                toCreateSize++;
            }
            // Generate structure nodes
            while (toCreateSize > 1) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                for (uint32_t i = 0; i < 2; i++) {
                    snode c = toCreate.front();
                    toCreate.pop_front();
                    toCreateSize--;
                    if (c.children.size() == 2)
                        sim.getNode(c.addr).generateSNode(n.addr, c.children[0], c.children[1], c.level);
                    else
                        sim.getNode(c.addr).generateSNode(n.addr, c.children[0], c.children[1], c.children[2], c.level);
                    n.children.push_back(c.addr);
                    n.level = c.level + 1;
                }
                toCreate.push_back(n);
                toCreateSize++;
            }
            // Root node
            snode root = toCreate.front();
            sim.getNode(root.addr).generateSNode(root.addr, root.children[0], root.children[1], root.level);
        } else {
            // We also need some level 1 nodes with 3 children
            uint32_t twoChildren = 0;
            if (numNodes % 3 == 1) twoChildren = 2;
            else if (numNodes % 3 == 2) twoChildren = 1;
            uint32_t threeChildren = (numNodes - twoChildren * 2) / 3;
            uint32_t level1threeChildren = threeChildren + twoChildren - p2NumNodes / 2;
            // Generate ResourceNodes
            // Generate groups of 3
            unsigned int rpos = 0;
            unsigned int toCreateSize = 0;
            for (uint32_t spos = 0; spos < threeChildren; spos++) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                n.level = 0;
                for (uint32_t i = 0; i < 3; rpos++, i++) {
                    sim.getNode(rpos).generateRNode(n.addr);
                    n.children.push_back(rpos);
                }
                toCreate.push_back(n);
                toCreateSize++;
            }
            // Generate groups of 2
            for (uint32_t spos = 0; spos < twoChildren; spos++) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                n.level = 0;
                for (uint32_t i = 0; i < 2; rpos++, i++) {
                    sim.getNode(rpos).generateRNode(n.addr);
                    n.children.push_back(rpos);
                }
                toCreate.push_back(n);
                toCreateSize++;
            }
            // Generate structure nodes
            // first the level 1 nodes with 3 children
            for (uint32_t j = 0; j < level1threeChildren; j++) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                for (uint32_t i = 0; i < 3; i++) {
                    snode c = toCreate.front();
                    toCreate.pop_front();
                    toCreateSize--;
                    if (c.children.size() == 2)
                        sim.getNode(c.addr).generateSNode(n.addr, c.children[0], c.children[1], c.level);
                    else
                        sim.getNode(c.addr).generateSNode(n.addr, c.children[0], c.children[1], c.children[2], c.level);
                    n.children.push_back(c.addr);
                    n.level = c.level + 1;
                }
                toCreate.push_back(n);
                toCreateSize++;
            }
            while (toCreateSize > 1) {
                snode n;
                n.addr = treeNodes.back();
                treeNodes.pop_back();
                for (uint32_t i = 0; i < 2; i++) {
                    snode c = toCreate.front();
                    toCreate.pop_front();
                    toCreateSize--;
                    if (c.children.size() == 2)
                        sim.getNode(c.addr).generateSNode(n.addr, c.children[0], c.children[1], c.level);
                    else
                        sim.getNode(c.addr).generateSNode(n.addr, c.children[0], c.children[1], c.children[2], c.level);
                    n.children.push_back(c.addr);
                    n.level = c.level + 1;
                }
                toCreate.push_back(n);
                toCreateSize++;
            }
            // Root node
            snode root = toCreate.front();
            sim.getNode(root.addr).generateSNode(root.addr, root.children[0], root.children[1], root.level);
        }

        // Prevent any timer from running the simulation
        sim.stop();
    }

    void postEnd() {
        PeerCompNode::saveState(property);
    }
};
REGISTER_SIMULATION_CASE(createTree);
