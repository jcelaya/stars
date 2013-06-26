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
#include "Simulator.hpp"
#include "SimulationCase.hpp"
#include "SimOverlayBranch.hpp"
#include "SimOverlayLeaf.hpp"
#include "IBPDispatcher.hpp"
#include "MMPDispatcher.hpp"
#include "DPDispatcher.hpp"
#include "FSPDispatcher.hpp"
using std::vector;


class CreateSimOverlay : public SimulationCase {
public:
    CreateSimOverlay(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const std::string getName() { return std::string("create_sim_overlay"); }

    virtual void preStart() {
        // NOTE: outside MSG_main, do not call Simulator::getCurrentNode() !!
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
        std::list<StarsNode *> sortedNodes;
        sortedNodes.push_back(&sim.getNode(currentLevel[0]));
        for (std::list<StarsNode *>::iterator i = sortedNodes.begin(); i != sortedNodes.end(); ++i) {
            if (!(**i).getBranch().isLeftLeaf())
                sortedNodes.push_back(&sim.getNode((**i).getBranch().getLeftAddress().getIPNum()));
            if (!(**i).getBranch().isRightLeaf())
                sortedNodes.push_back(&sim.getNode((**i).getBranch().getRightAddress().getIPNum()));
        }
        for (std::list<StarsNode *>::reverse_iterator i = sortedNodes.rbegin(); i != sortedNodes.rend(); ++i)
            (**i).buildDispatcher();

        // Prevent any timer from running the simulation
        sim.stop();
    }
};
REGISTER_SIMULATION_CASE(CreateSimOverlay);
