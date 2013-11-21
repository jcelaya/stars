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
    static const char * prefixDash[2];
    static const char * prefixNoDash[2];

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
        Logger::msg("Sim.Tree", WARN, roots.size(), " different trees.");
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
                for (int child : {0, 1}) {
                    unsigned int childAddr = branch.getChildAddress(child).getIPNum();
                    StarsNode & childNode = Simulator::getInstance().getNode(childAddr);
                    unsigned int targetAddr = (branch.isLeaf(child) ?
                            childNode.getLeaf().getFatherAddress()
                            : childNode.getBranch().getFatherAddress()).getIPNum();
                    if (targetAddr != i) {
                        Logger::msg("Sim.Tree", ERROR, "Link mismatch: father of ",
                                childAddr, " is ", targetAddr, " and should be ", i);
                    }
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
            Logger::msg("Sim.Tree", WARN, "Structure tree:");
            StarsNode & rootNode = Simulator::getInstance().getNode(root);
            if (rootNode.getBranch().inNetwork()) {
                showRecursiveStructure(rootNode, -1, "");
            }
        }
    }

    void showRecursiveStructure(const StarsNode & node, unsigned int level, const std::string & prefix) {
        SimOverlayBranch & branch = static_cast<SimOverlayBranch &>(node.getBranch());
        Logger::msg("Sim.Tree", WARN, prefix, "B@", node.getLocalAddress().getIPNum(), ": ", branch);
        if (level) {
            for (int c : {0, 1}) {
                // Right child
                StarsNode & child = Simulator::getInstance().getNode(branch.getChildAddress(c).getIPNum());
                Logger::msg("Sim.Tree", WARN, prefix, prefixDash[c], branch.getChildZone(c));

                if (!branch.isLeaf(c)) {
                    showRecursiveStructure(child, level - 1, prefix + prefixNoDash[c]);
                } else {
                    Logger::msg("Sim.Tree", WARN, prefix, prefixNoDash[c], "L@", branch.getChildAddress(c).getIPNum(), ": ",
                            static_cast<SimOverlayLeaf &>(child.getLeaf()));
                }
            }
        }
    }

    void showInfoTree(std::set<unsigned int> & roots) {
        for (auto root : roots) {
            Logger::msg("Sim.Tree", WARN, "Information tree:");
            Logger::setIndentActive(false);
            StarsNode & rootNode = Simulator::getInstance().getNode(root);
            if (rootNode.getBranch().inNetwork()) {
                showRecursiveInfo(rootNode, -1, "");
            }
        }
    }

    void showRecursiveInfo(const StarsNode & node, unsigned int level, const std::string & prefix) {
        std::shared_ptr<AvailabilityInformation> info = node.getDisp().getBranchInfo();
        SimOverlayBranch & branch = static_cast<SimOverlayBranch &>(node.getBranch());
        if (info.get())
            Logger::msg("Sim.Tree", WARN, prefix, "B@", node.getLocalAddress(), ": ", *info);
        else
            Logger::msg("Sim.Tree", WARN, prefix, "B@", node.getLocalAddress(), ": ", " ?");
        if (level) {
            for (int c : {0, 1}) {
                StarsNode & child = Simulator::getInstance().getNode(branch.getChildAddress(c).getIPNum());
                info = node.getDisp().getChildInfo(c);
                if (info.get())
                    Logger::msg("Sim.Tree", WARN, prefix, prefixDash[c], *info);
                else
                    Logger::msg("Sim.Tree", WARN, prefix, prefixDash[c], " ?");

                if (!branch.isLeaf(c)) {
                    showRecursiveInfo(child, level - 1, prefix + prefixNoDash[c]);
                } else {
                    Logger::msg("Sim.Tree", WARN, prefix, prefixNoDash[c], "L@", branch.getChildAddress(c), ": ",
                            child, ' ', *child.getSch().getAvailability());
                }
            }
        }
    }

    void checkInfoTree() {

    }
};
REGISTER_SIMULATION_CASE(networkCheck);

const char * networkCheck::prefixDash[2] = {"  |- ", "  \\- "};
const char * networkCheck::prefixNoDash[2] = {"  |  ", "     "};
