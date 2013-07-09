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

#include "Simulator.hpp"
#include "SimulationCase.hpp"
#include "StructureNode.hpp"
#include "TaskDescription.hpp"
#include "TrafficStatistics.hpp"
#include "InsertCommandMsg.hpp"
#include "Logger.hpp"
using namespace std;
using namespace boost::posix_time;



/// Test cases

// Copy-Paste to create new test hehehe
//class SimulatorTesting : public PeerCompSimulation<SimulatorTesting> {
//public:
//	SimulatorTesting(const Properties & p) : PeerCompSimulation<SimulatorTesting>(p) {
//		// Prepare the properties
//	}
//
//	static const string getName() { return string("SimulatorTesting"); }
//
//	void preStart() {
//		// Before running simulation
//	}
//
//	void postEnd() {
//		// After ending simulation
//	}
//};
//REGISTER_SIMULATION_CASE(SimulatorTesting);


///// Scenario 1: Create network
//class networkCreation : public PeerCompSimulation<networkCreation> {
//public:
//	networkCreation(const Properties & p) : PeerCompSimulation<networkCreation>(p) {
//		// Prepare the properties
//		property["num_nodes"] = "1";
//		property["fanout"] = "2";
//	}
//
//	static const string getName() { return string("networkCreation"); }
//
//	void preStart() {
//		InsertCommandMsg icm;
//		icm.setWhere(CommAddress(0));
//		sim.sendMessage(0, 0, icm.clone());
//	}
//
//	void postEnd() {
//		sim.showStatistics();
//		StarsNode::showTree(log4cpp::Priority::INFO);
//	}
//};
//REGISTER_SIMULATION_CASE(networkCreation);
//
//
///// Scenario 2: Add execution nodes to the network.
//class oneInsertion : public PeerCompSimulation<oneInsertion> {
//public:
//	oneInsertion(const Properties & p) : PeerCompSimulation<oneInsertion>(p) {
//		// Prepare the properties
//		property["num_nodes"] = "2";
//		property["fanout"] = "2";
//	}
//
//	static const string getName() { return string("oneInsertion"); }
//
//	void preStart() {
//		InsertCommandMsg icm;
//		icm.setWhere(CommAddress(0));
//		sim.sendMessage(0, 0, icm.clone());
//		sim.sendMessage(1, 1, icm.clone());
//	}
//
//	void postEnd() {
//		sim.showStatistics();
//		StarsNode::showTree(log4cpp::Priority::INFO);
//	}
//};
//REGISTER_SIMULATION_CASE(oneInsertion);
//
//
///// Scenario 3: Add execution nodes to the network until the root splits.
//class firstRootSplit : public PeerCompSimulation<firstRootSplit> {
//public:
//	firstRootSplit(const Properties & p) : PeerCompSimulation<firstRootSplit>(p) {
//		// Prepare the properties
//		property["num_nodes"] = "4";
//		property["fanout"] = "2";
//	}
//
//	static const string getName() { return string("firstRootSplit"); }
//
//	void preStart() {
//		// Before running simulation
//		InsertCommandMsg icm;
//		icm.setWhere(CommAddress(0));
//		sim.sendMessage(0, 0, icm.clone());
//		sim.sendMessage(1, 1, icm.clone());
//		sim.sendMessage(2, 2, icm.clone());
//		sim.sendMessage(3, 3, icm.clone());
//	}
//
//	void postEnd() {
//		// After ending simulation
//		sim.showStatistics();
//		StarsNode::showTree(log4cpp::Priority::INFO);
//	}
//};
//REGISTER_SIMULATION_CASE(firstRootSplit);
//
//
///// Scenario 4: Add a lot of execution nodes to the network
//class sequentialNodeInsertion : public PeerCompSimulation<sequentialNodeInsertion> {
//	unsigned int nextInsert;
//	unsigned int numNodes;
//	InsertCommandMsg icm;
//
//public:
//	sequentialNodeInsertion(const Properties & p) : PeerCompSimulation<sequentialNodeInsertion>(p) {
//		// Prepare the properties
//	}
//
//	static const string getName() { return string("sequentialNodeInsertion"); }
//
//	void preStart() {
//		// Before running simulation
//		numNodes = sim.getNumNodes();
//		nextInsert = 1;
//		sim.registerHandler(*this);
//
//		icm.setWhere(CommAddress(0));
//		LogMsg("Sim.St"(), INFO) << "Inserting node E0";
//		sim.sendMessage(0, 0, icm.clone());
//	}
//
//	void afterEvent(const Simulator::Event & ev) {
//		if (sim.emptyEventQueue() && nextInsert < numNodes) {
//			LogMsg("Sim.St"(), INFO) << "";
//			LogMsg("Sim.St"(), INFO) << "";
//			LogMsg("Sim.St"(), INFO) << "Inserting node E" << nextInsert;
//			StarsNode::showTree();
//			sim.sendMessage(nextInsert, nextInsert, icm.clone());
//			nextInsert++;
//		}
//	}
//
//	void postEnd() {
//		LogMsg("Sim.St"(), INFO) << "";
//		LogMsg("Sim.St"(), INFO) << "";
//		// After ending simulation
//		sim.showStatistics();
//		StarsNode::showTree(log4cpp::Priority::INFO);
//		StarsNode::checkTree();
//		StarsNode::saveState();
//	}
//};
//REGISTER_SIMULATION_CASE(sequentialNodeInsertion);
//
//
///// Scenario 6: Add a lot of execution nodes to the network in a random order
//class randomNodeInsertion : public PeerCompSimulation<randomNodeInsertion> {
//	int * ee;
//	unsigned int numNodes;
//	unsigned int nextInsert;
//	InsertCommandMsg icm;
//
//public:
//	randomNodeInsertion(const Properties & p) : PeerCompSimulation<randomNodeInsertion>(p) {
//		// Prepare the properties
//	}
//
//	static const string getName() { return string("randomNodeInsertion"); }
//
//	void preStart() {
//		// Before running simulation
//		numNodes = sim.getNumNodes();
//		nextInsert = 1;
//		sim.registerHandler(*this);
//
//		ee = new int[numNodes];
//		for (unsigned int i = 0; i < numNodes; i++) ee[i] = i;
//
//		// Shuffle it
//		for (unsigned int i = 1; i < numNodes - 1; i++) {
//			// random is a number between i + 1 and numNodes - 1
//			int random = Simulator::uniform((int)i + 1, (int)numNodes - 1);
//			int tmp = ee[random];
//			ee[random] = ee[i];
//			ee[i] = tmp;
//		}
//		// The first one is always 0 ;P
//
//		icm.setWhere(CommAddress(0));
//
//		LogMsg("Sim.St"(), DEBUG) << "Inserting node E0";
//		sim.sendMessage(0, 0, icm.clone());
//	}
//
//	void afterEvent(const Simulator::Event & ev) {
//		if (sim.emptyEventQueue() && nextInsert < numNodes) {
//			LogMsg("Sim.St"(), INFO) << "";
//			LogMsg("Sim.St"(), INFO) << "";
//			int addr = ee[nextInsert];
//			LogMsg("Sim.St"(), DEBUG) << "Inserting node E" << addr;
//			sim.sendMessage(addr, addr, icm.clone());
//			nextInsert++;
//		}
//	}
//
//	void postEnd() {
//		LogMsg("Sim.St"(), INFO) << "";
//		LogMsg("Sim.St"(), INFO) << "";
//		// After ending simulation
//		delete[] ee;
//		sim.showStatistics();
//		StarsNode::showTree(log4cpp::Priority::INFO);
//		StarsNode::checkTree();
//		StarsNode::saveState();
//	}
//};
//REGISTER_SIMULATION_CASE(randomNodeInsertion);
//
//
///// Scenario 7: Add a lot of execution nodes to the network in a concurrent way
//class concurrentNodeInsertion : public PeerCompSimulation<concurrentNodeInsertion> {
//	InsertCommandMsg icm;
//	unsigned int numNodes;
//	bool firstInsert;
//public:
//	concurrentNodeInsertion(const Properties & p) : PeerCompSimulation<concurrentNodeInsertion>(p) {
//		// Prepare the properties
//	}
//
//	static const string getName() { return string("concurrentNodeInsertion"); }
//
//	void preStart() {
//		// Before running simulation
//		numNodes = sim.getNumNodes();
//		firstInsert = true;
//		sim.registerHandler(*this);
//
//		LogMsg("Sim.St"(), DEBUG) << "Inserting node E0";
//		icm.setWhere(CommAddress(0));
//		sim.sendMessage(0, 0, icm.clone());
//	}
//
//	void afterEvent(const Simulator::Event & ev) {
//		if (firstInsert && sim.emptyEventQueue()) {
//			LogMsg("Sim.St"(), DEBUG) << "";
//			LogMsg("Sim.St"(), DEBUG) << "";
//			LogMsg("Sim.St"(), DEBUG) << "Inserting the other nodes";
//			LogMsg("Sim.St"(), DEBUG) << "";
//			for (unsigned int i = 1; i < numNodes; i++ ) {
//				LogMsg("Sim.St"(), DEBUG) << "Inserting node E" << i;
//				sim.sendMessage(i, i, icm.clone());
//			}
//			firstInsert = false;
//		}
//	}
//
//	void postEnd() {
//		// After ending simulation
//		sim.showStatistics();
//		StarsNode::showTree(log4cpp::Priority::INFO);
//		StarsNode::checkTree();
//		StarsNode::saveState();
//	}
//};
//REGISTER_SIMULATION_CASE(concurrentNodeInsertion);


/// Scenario 9: Create random tree at zero
class createSimTree : public SimulationCase {

public:
    createSimTree(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const string getName() { return string("createSimTree"); }

    void preStart() {
        Simulator & sim = Simulator::getInstance();
        uint16_t port = ConfigurationManager::getInstance().getPort();
        uint32_t numNodes = sim.getNumNodes();
        uint32_t p2NumNodes = 1;
        for (uint32_t i = numNodes; i > 1; i >>= 1)
            p2NumNodes <<= 1;
        uint32_t l1NumNodes = numNodes - p2NumNodes;

        // Select l1NumNodes nodes that will have an additional level
        vector<bool> additionalLevel(p2NumNodes, false);
        for (uint32_t i = 0; i < l1NumNodes; ++i)
            additionalLevel[i] = true;
        random_shuffle(additionalLevel.begin(), additionalLevel.end());

        // Prepare first level and additional level
        vector<uint32_t> currentLevel(p2NumNodes);
        vector<uint32_t> availBranches(p2NumNodes);
        for (uint32_t i = 0, j = 0; i < p2NumNodes; ++i, ++j) {
            currentLevel[i] = j;
            if (additionalLevel[i]) {
                // Setup these SimOverlayLeafs
                CommAddress father(j, port);
                static_cast<SimOverlayLeaf &>(sim.getNode(j).getLeaf()).setFatherAddress(father);
                static_cast<SimOverlayLeaf &>(sim.getNode(j + 1).getLeaf()).setFatherAddress(father);
                // And the SimOverlayBranch
                static_cast<SimOverlayBranch &>(sim.getNode(j).getBranch()).build(CommAddress(j, port), false, CommAddress(j + 1, port), false);
                ++j;
            }
            availBranches[i] = j;
        }
        // Shuffle branches
        random_shuffle(availBranches.begin(), availBranches.end());
        uint32_t nextAvail = 0;

        // First level
        for (size_t i = 0, j = 0; i < p2NumNodes; i += 2, ++j) {
            uint32_t leftAddr = currentLevel[i], rightAddr = currentLevel[i + 1], fatherAddr = availBranches[nextAvail++];
            currentLevel[j] = fatherAddr;
            // Setup these SimOverlayLeafs
            CommAddress father(fatherAddr, port);
            if (additionalLevel[i])
                static_cast<SimOverlayBranch &>(sim.getNode(leftAddr).getBranch()).setFatherAddress(father);
            else {
                static_cast<SimOverlayLeaf &>(sim.getNode(leftAddr).getLeaf()).setFatherAddress(father);
            }
            if (additionalLevel[i + 1])
                static_cast<SimOverlayBranch &>(sim.getNode(rightAddr).getBranch()).setFatherAddress(father);
            else {
                static_cast<SimOverlayLeaf &>(sim.getNode(rightAddr).getLeaf()).setFatherAddress(father);
            }
            // And the SimOverlayBranch
            static_cast<SimOverlayBranch &>(sim.getNode(fatherAddr).getBranch()).build(
                    CommAddress(leftAddr, port), additionalLevel[i], CommAddress(rightAddr, port), additionalLevel[i + 1]);
        }
        p2NumNodes >>= 1;

        // Next levels
        while (p2NumNodes > 1) {
            for (size_t i = 0, j = 0; i < p2NumNodes; ++i, ++j) {
                uint32_t leftAddr = currentLevel[i], rightAddr = currentLevel[++i], fatherAddr = availBranches[nextAvail++];
                currentLevel[j] = fatherAddr;
                // Setup these SimOverlayLeafs
                CommAddress father(fatherAddr, port);
                static_cast<SimOverlayBranch &>(sim.getNode(leftAddr).getBranch()).setFatherAddress(father);
                static_cast<SimOverlayBranch &>(sim.getNode(rightAddr).getBranch()).setFatherAddress(father);
                // And the SimOverlayBranch
                static_cast<SimOverlayBranch &>(sim.getNode(fatherAddr).getBranch()).build(
                        CommAddress(leftAddr, port), true, CommAddress(rightAddr, port), true);
            }
            p2NumNodes >>= 1;
        }

        // Build Dispatchers
        list<StarsNode *> sortedNodes;
        sortedNodes.push_back(&sim.getNode(currentLevel[0]));
        for (list<StarsNode *>::iterator i = sortedNodes.begin(); i != sortedNodes.end(); ++i) {
            if (!(**i).getBranch().isLeftLeaf())
                sortedNodes.push_back(&sim.getNode((**i).getBranch().getLeftAddress().getIPNum()));
            if (!(**i).getBranch().isRightLeaf())
                sortedNodes.push_back(&sim.getNode((**i).getBranch().getRightAddress().getIPNum()));
        }
        for (list<StarsNode *>::reverse_iterator i = sortedNodes.rbegin(); i != sortedNodes.rend(); ++i)
            (**i).buildDispatcher();
        for (auto i = sortedNodes.begin(); i != sortedNodes.end(); ++i)
            (**i).buildDispatcherDown();

        // Prevent any timer from running the simulation
        sim.stop();
    }

    void postEnd() {
    }
};
REGISTER_SIMULATION_CASE(createSimTree);
