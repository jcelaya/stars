/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
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

#ifndef TRANSACTIONMSG_H_
#define TRANSACTIONMSG_H_

#include "BasicMsg.hpp"


// Transaction type
typedef uint64_t TransactionId;
#define NULL_TRANSACTION_ID 0
TransactionId createRandomId();


class TransactionMsg: public BasicMsg {
public:
    MESSAGE_SUBCLASS(TransactionMsg);
    
    TransactionMsg(TransactionId trans = 0) : transaction(trans) {}

    // Getters and Setters

    /**
     * Returns the transaction ID of this message.
     * @return Transaction ID.
     */
    TransactionId getTransactionId() const {
        return transaction;
    }

    /**
     * Sets the transaction ID of this message.
     * @param t The new transaction ID.
     */
    void setTransactionId(TransactionId t) {
        transaction = t;
    }

    /**
     * Provides a textual representation of this object. Just one line, no endline char.
     * @param os Output stream where to write the representation.
     */
    virtual void output(std::ostream& os) const {
        os << "tid(" << transaction << ')';
    }
    
    MSGPACK_DEFINE(transaction);
protected:
    TransactionId transaction;   ///< Transaction ID, to relate messages to each other
};

#endif /* TRANSACTIONMSG_H_ */
