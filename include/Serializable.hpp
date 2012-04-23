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

#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>


/**
 * Defines the interface for a serialization method
 */
#define SRLZ_API friend class boost::serialization::access; template<class Archive>

/**
 * The method definition to serialize/unserialize an object.
 * It must be used preceded by SRLZ_API
 */
#define SRLZ_METHOD() void serialize(Archive & ar, const unsigned int version)

#define IS_LOADING() typename Archive::is_loading()

#define IS_SAVING() typename Archive::is_saving()

#define SERIALIZE_BASE(baseclass) \
	boost::serialization::base_object<baseclass>(*this)

#endif /*SERIALIZABLE_H_*/
