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

#ifndef ACKMSG_H_
#define ACKMSG_H_

#include "TransactionMsg.hpp"


/**
 * Acknowledgement message in a transaction.
 */
class AckMsg : public TransactionMsg {
public:
    MESSAGE_SUBCLASS(AckMsg);

    /// Constructor
    AckMsg(TransactionId trans = NULL_TRANSACTION_ID) : TransactionMsg(trans), forRN(false), fromRN(false) {}

    bool isForRN() const {
        return forRN;
    }

    void setForRN(bool rn) {
        forRN = rn;
    }

    bool isFromRN() const {
        return fromRN;
    }

    void setFromRN(bool rn) {
        fromRN = rn;
    }

    // This is documented in BasicMsg
    void output(std::ostream& os) const {}

    MSGPACK_DEFINE((TransactionMsg &)*this, forRN, fromRN);
private:
    bool forRN;    ///< To say whether this message is for the ResourceNode or the StructureNode
    bool fromRN;   ///< To say whether this message comes from the ResourceNode or the StructureNode
};

#endif /*ACKMSG_H_*/
