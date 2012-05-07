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

#ifndef CHECKMSG_HPP_
#define CHECKMSG_HPP_

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/shared_ptr.hpp>
#include "portable_binary_iarchive.hpp"
#include "portable_binary_oarchive.hpp"
#include "BasicMsg.hpp"
#include "CommAddress.hpp"
using boost::shared_ptr;


class CheckMsgMethod {
public:
	template<class Message>
	static unsigned int check(const Message & msg, shared_ptr<Message> & copy) {
		std::stringstream ss;
		portable_binary_oarchive oa(ss);
		shared_ptr<BasicMsg> out(msg.clone()), in;
		// Check clone
		BOOST_REQUIRE(boost::dynamic_pointer_cast<Message>(out).get());
		//BOOST_CHECK(msg == *boost::dynamic_pointer_cast<Message>(out));
		oa << out;
		unsigned int size = ss.tellp();
		BOOST_TEST_MESSAGE(msg.getName() << " of size " << size << " bytes.");
		portable_binary_iarchive ia(ss);
		ia >> in;
		copy = boost::dynamic_pointer_cast<Message>(in);
		BOOST_REQUIRE(copy.get());
		//BOOST_CHECK(msg == *copy);
		return size;
	}
};

#endif /* CHECKMSG_HPP_ */
