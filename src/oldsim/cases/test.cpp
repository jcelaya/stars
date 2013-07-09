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

#include <unistd.h>
#include <set>
#include "SimulationCase.hpp"
#include "Simulator.hpp"


class noop : public SimulationCase {

public:
    noop(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const std::string getName() { return std::string("noop"); }

    void preStart() {
        int w = property("wait", 5);
        sleep(w);
    }

    bool doContinue() const {
        return false;
    }
};
REGISTER_SIMULATION_CASE(noop);


class sigsev : public SimulationCase {

public:
    sigsev(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const std::string getName() { return std::string("sigsev"); }

    void preStart() {
        int w = property("wait", 5);
        sleep(w);
        *((int *)0) = 0;
    }

    bool doContinue() const {
        return *((int *)0) == 0;
    }
};
REGISTER_SIMULATION_CASE(sigsev);


class networkCheck : public SimulationCase {
public:
    networkCheck(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const std::string getName() { return std::string("networkCheck"); }

    void preStart() {
        // Prevent any timer from running the simulation
        Simulator::getInstance().stop();
    }

    void postEnd() {
        std::set<unsigned int> roots = getRoots();
        LogMsg("Sim.Tree", WARN) << roots.size() << " different trees.";
        showStructureTree(roots);
        checkStructureTree();
        showInfoTree(roots);
        checkInfoTree();
    }

    void checkStructureTree() {
        for (unsigned int i = 0; i < Simulator::getInstance().getNumNodes(); ++i) {
            StarsNode & node = Simulator::getInstance().getNode(i);
            OverlayBranch & branch = node.getBranch();
            if (branch.inNetwork()) {
                unsigned int rightAddr = branch.getRightAddress().getIPNum();
                StarsNode & rightNode = Simulator::getInstance().getNode(rightAddr);
                unsigned int targetAddr = (branch.isRightLeaf() ?
                        rightNode.getLeaf().getFatherAddress()
                        : rightNode.getBranch().getFatherAddress()).getIPNum();
                if (targetAddr != i) {
                    LogMsg("Sim.Tree", ERROR) << "Link mismatch: father of " << rightAddr << " is " << targetAddr
                            << " and should be " << i;
                }
                unsigned int leftAddr = branch.getRightAddress().getIPNum();
                StarsNode & leftNode = Simulator::getInstance().getNode(leftAddr);
                targetAddr = (branch.isRightLeaf() ?
                        leftNode.getLeaf().getFatherAddress()
                        : leftNode.getBranch().getFatherAddress()).getIPNum();
                if (targetAddr != i) {
                    LogMsg("Sim.Tree", ERROR) << "Link mismatch: father of " << leftAddr << " is " << targetAddr
                            << " and should be " << i;
                }
            }
        }
    }

    std::set<unsigned int> getRoots() {
        std::set<unsigned int> roots;
        for (unsigned int i = 0; i < Simulator::getInstance().getNumNodes(); ++i ) {
            roots.insert(roots.begin(), getRoot(Simulator::getInstance().getNode(i)));
        }
        return roots;
    }

    unsigned int getRoot(const StarsNode & node) const {
        const CommAddress & father = node.getBranch().inNetwork() ? node.getBranch().getFatherAddress() : node.getLeaf().getFatherAddress();
        if (father != CommAddress()) return getRoot(Simulator::getInstance().getNode(father.getIPNum()));
        else return node.getLocalAddress().getIPNum();
    }

    void showStructureTree(std::set<unsigned int> & roots) {
        for (auto root : roots) {
            LogMsg("Sim.Tree", WARN) << "Structure tree:";
            StarsNode & rootNode = Simulator::getInstance().getNode(root);
            if (rootNode.getBranch().inNetwork()) {
                showRecursiveStructure(rootNode, -1, "");
            }
        }
    }

    void showRecursiveStructure(const StarsNode & node, unsigned int level, const std::string & prefix) {
        SimOverlayBranch & branch = static_cast<SimOverlayBranch &>(node.getBranch());
        LogMsg("Sim.Tree", WARN) << prefix << "B@" << node.getLocalAddress().getIPNum() << ": " << branch;
        if (level) {
            // Right child
            StarsNode & rightChild = Simulator::getInstance().getNode(branch.getRightAddress().getIPNum());
            LogMsg("Sim.Tree", WARN) << prefix << "  |- " << branch.getRightZone();

            if (!branch.isRightLeaf()) {
                showRecursiveStructure(rightChild, level - 1, prefix + "  |  ");
            } else {
                LogMsg("Sim.Tree", WARN) << prefix << "  |  " << "L@" << branch.getRightAddress().getIPNum() << ": "
                        << static_cast<SimOverlayLeaf &>(rightChild.getLeaf());
            }

            // Left child
            StarsNode & leftChild = Simulator::getInstance().getNode(branch.getLeftAddress().getIPNum());
            LogMsg("Sim.Tree", WARN) << prefix << "  \\- " << branch.getLeftZone();

            if (!branch.isLeftLeaf()) {
                showRecursiveStructure(leftChild, level - 1, prefix + "     ");
            } else {
                LogMsg("Sim.Tree", WARN) << prefix << "     " << "L@" << branch.getLeftAddress().getIPNum() << ": "
                        << static_cast<SimOverlayLeaf &>(leftChild.getLeaf());
            }
        }
    }

    void showInfoTree(std::set<unsigned int> & roots) {
        for (auto root : roots) {
            LogMsg("Sim.Tree", WARN) << "Information tree:";
            StarsNode & rootNode = Simulator::getInstance().getNode(root);
            if (rootNode.getBranch().inNetwork()) {
                showRecursiveInfo(rootNode, -1, "");
            }
        }
    }

    void showRecursiveInfo(const StarsNode & node, unsigned int level, const std::string & prefix) {
        boost::shared_ptr<AvailabilityInformation> info = node.getDisp().getBranchInfo();
        SimOverlayBranch & branch = static_cast<SimOverlayBranch &>(node.getBranch());
        if (info.get())
            LogMsg("Sim.Tree", WARN) << prefix << "B@" << node.getLocalAddress().getIPNum() << ": " << *info;
        else
            LogMsg("Sim.Tree", WARN) << prefix << "B@" << node.getLocalAddress().getIPNum() << ": " << " ?";
        if (level) {
            // Right child
            StarsNode & rightChild = Simulator::getInstance().getNode(branch.getRightAddress().getIPNum());
            info = node.getDisp().getRightChildInfo();
            if (info.get())
                LogMsg("Sim.Tree", WARN) << prefix << "  |- " << *info;
            else
                LogMsg("Sim.Tree", WARN) << prefix << "  |- " << " ?";

            if (!branch.isRightLeaf()) {
                showRecursiveInfo(rightChild, level - 1, prefix + "  |  ");
            } else {
                LogMsg("Sim.Tree", WARN) << prefix << "  |  " << "L@" << branch.getRightAddress().getIPNum() << ": "
                        << rightChild << ' ' << rightChild.getSch().getAvailability();
            }

            // Left child
            StarsNode & leftChild = Simulator::getInstance().getNode(branch.getLeftAddress().getIPNum());
            info = node.getDisp().getLeftChildInfo();
            if (info.get())
                LogMsg("Sim.Tree", WARN) << prefix << "  \\- " << *info;
            else
                LogMsg("Sim.Tree", WARN) << prefix << "  \\- " << " ?";

            if (!branch.isLeftLeaf()) {
                showRecursiveInfo(leftChild, level - 1, prefix + "     ");
            } else {
                LogMsg("Sim.Tree", WARN) << prefix << "     " << "L@" << branch.getLeftAddress().getIPNum() << ": "
                        << leftChild << ' ' << leftChild.getSch().getAvailability();
            }
        }
    }

    void checkInfoTree() {

    }
};
REGISTER_SIMULATION_CASE(networkCheck);
