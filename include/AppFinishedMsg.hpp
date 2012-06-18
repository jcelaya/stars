/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2010 Javier Celaya
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

#ifndef APPFINISHEDMSG_H_
#define APPFINISHEDMSG_H_

#include "BasicMsg.hpp"

class AppFinishedMsg: public BasicMsg {
    int64_t appId;

public:
    MESSAGE_SUBCLASS(AppFinishedMsg);
    
    /**
     * Obtains the app ID
     */
    int64_t getAppId() const {
        return appId;
    }

    /**
     * Sets the app ID
     */
    void setAppId(int64_t v) {
        appId = v;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}
    
    MSGPACK_DEFINE();
};

#endif /* APPFINISHEDMSG_H_ */
