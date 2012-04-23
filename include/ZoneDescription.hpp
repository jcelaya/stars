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

#ifndef ZONEDESCRIPTION_H_
#define ZONEDESCRIPTION_H_

#include <list>
#include <ostream>
#include <boost/shared_ptr.hpp>
#include "CommAddress.hpp"


/**
 * A description of a zone of the tree. It manages the data associated to a
 * tree zone, like the resource information covered by the nodes hanging from it.
 * All members are supposed to be non-null, so be sure to set them before using any
 * other method.
 */
class ZoneDescription {
	/// Set the basic elements for a Serializable descendant
	SRLZ_API SRLZ_METHOD() {
		ar & minAddr;
		ar & maxAddr;
		ar & availableStrNodes;
	}

	CommAddress minAddr;          ///< The minimum address in the zone
	CommAddress maxAddr;          ///< The maximum address in the zone
	uint32_t availableStrNodes;   ///< The number of available structure nodes

public:
	/// Default constructor
	ZoneDescription() : availableStrNodes(0) {}
	/// Copy constructor
	ZoneDescription(const ZoneDescription & copy) : minAddr(copy.minAddr), maxAddr(copy.maxAddr),
			availableStrNodes(copy.availableStrNodes) {}
	/// Constructor from a list of zones
	ZoneDescription(const std::list<boost::shared_ptr<ZoneDescription> > & zones) { aggregate(zones); }

	/**
	 * Equality operator.
	 * @param r The right operand.
	 * @return True when both objects are equal.
	 */
	bool operator==(const ZoneDescription & r) const;

	/**
	 * Containment operator, between a node and this zone.
	 * @param src The address of the node the information belongs to.
	 * @return True if this zone contains both the address and the information of that node.
	 */
	bool contains(const CommAddress & src) const;

	/**
	 * Distance operator between this zone and a node. It is a real number between 0.0 and infinite.
	 * @param src The address of the node the information belongs to.
	 * @return The distance.
	 */
	double distance(const CommAddress & src) const;

	/**
	 * Distance operator between two zones. It is a real number between 0.0 and infinite.
	 * @param r The right operator.
	 * @return The distance.
	 */
	double distance(const ZoneDescription & r) const;

	/**
	 * Aggregates a list of ZoneDescription objects together and leaves the result
	 * in this object.
	 * @param zones The list of zones to aggregate.
	 */
	void aggregate(const std::list<boost::shared_ptr<ZoneDescription> > & zones);

	// Getters and Setters

	/**
	 * Returns the minimum address contained in this zone.
	 * @return Minimum address contained in this zone.
	 */
	const CommAddress & getMinAddress() const { return minAddr; }

	/**
	 * Sets the minimum address contained in this zone.
	 * @param addr New minimum address contained in this zone.
	 */
	void setMinAddress(const CommAddress & addr) { minAddr = addr; }

	/**
	 * Returns the maximum address contained in this zone.
	 * @return Maximum address contained in this zone.
	 */
	const CommAddress & getMaxAddress() const { return maxAddr; }

	/**
	 * Sets the maximum address contained in this zone.
	 * @param addr New maximum address contained in this zone.
	 */
	void setMaxAddress(const CommAddress & addr) { maxAddr = addr; }

	/**
	 * Returns the number of available structure nodes in this zone.
	 * @return Number of available structure nodes.
	 */
	uint32_t getAvailableStrNodes() const { return availableStrNodes; }

	/**
	 * Sets the number of available structure nodes in this zone.
	 * @param newAvail New number of available structure nodes.
	 */
	void setAvailableStrNodes(uint32_t newAvail) { availableStrNodes = newAvail; }

	/**
	 * Returns whether this zone intersects another one.
	 */
	bool intersects(const ZoneDescription & r) {
		return contains(r.minAddr) || contains(r.maxAddr) ||
			r.contains(minAddr) || r.contains(maxAddr);
	}

	/**
	 * Provides a textual representation of a ZoneDescription instance
	 * @param os Output stream where to write the representation
	 * @param s ZoneDescription object to represent.
	 */
	friend std::ostream & operator<<(std::ostream& os, const ZoneDescription & s);
};

#endif /*ZONEDESCRIPTION_H_*/
