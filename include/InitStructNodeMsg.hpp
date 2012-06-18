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

#ifndef INITSTRUCTNODEMSG_H_
#define INITSTRUCTNODEMSG_H_

#include <stdexcept>
#include <vector>
#include "TransactionMsg.hpp"
#include "CommAddress.hpp"


/**
 * \brief Initialize Structure Node message.
 *
 * This message class holds all the information needed by a new StructureNode to initialise
 * itself and enter the network. This message is sent by another StructureNode when it wants
 * to split and has found an available StructureNode to be the father of part of its children.
 * This message contains the address of the new node's father, the addresses of its children
 * and the level of the tree it is going to be inserted in.
 */
class InitStructNodeMsg : public TransactionMsg {
public:
    MESSAGE_SUBCLASS(InitStructNodeMsg);
    
    /**
     * Default constructor, just sets the level to 0.
     */
    InitStructNodeMsg() : fatherValid(false), level(0) {}
    InitStructNodeMsg(const InitStructNodeMsg & copy) : TransactionMsg(copy),
            father(copy.father), fatherValid(copy.fatherValid), children(copy.children), level(copy.level) {}

    // Getters and Setters

    bool isFatherValid() const {
        return fatherValid;
    }

    /**
     * Returns the address of the node which is going to be the father of the receiver.
     * @return The address of the father node.
     */
    const CommAddress & getFather() const {
        return father;
    }

    /**
     * Sets the address of the father node.
     * @param p The address of the father node.
     */
    void setFather(const CommAddress & p) {
        father = p;
        fatherValid = true;
    }

    /**
     * Returns the level of the tree in which the receiver is going to be inserted.
     * @return The level of the tree.
     */
    uint32_t getLevel() const {
        return level;
    }

    /**
     * Sets the level of the tree where the receiver is going to be inserted in.
     * @param l The new level of the tree.
     */
    void setLevel(uint32_t l) {
        level = l;
    }

    /**
     * Returns the number of children addresses contained in this message.
     * @return The number of children.
     */
    unsigned int getNumChildren() const {
        return children.size();
    }

    /**
     * Returns a specific child address, given its number.
     * @param i Number of child to get address of.
     * @return Address of child i.
     */
    const CommAddress & getChild(unsigned int i) const throw(std::out_of_range) {
        if (i < 0 || i >= children.size()) {
            // It is an error to try to access a children with an out of range index.
            throw std::out_of_range("Nonexistent child index");
        }
        return children[i];
    }

    /**
     * Adds the address of a specific child.
     * @param c New address of last child.
     */
    void addChild(const CommAddress & c) {
        children.push_back(c);
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TransactionMsg &)*this, children, level, fatherValid, father);
private:
    CommAddress father;             ///< The address of the father node
    bool fatherValid;               ///< Whether the father's address should be taken into account
    std::vector<CommAddress> children;   ///< The addresses of the children nodes
    uint32_t level;                 ///< The level of the tree
};

#endif /*INITSTRUCTNODEMSG_H_*/
