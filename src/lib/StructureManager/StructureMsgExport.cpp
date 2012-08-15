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

#include "AckMsg.hpp"
#include "CommitMsg.hpp"
#include "InitStructNodeMsg.hpp"
#include "InsertCommandMsg.hpp"
#include "InsertMsg.hpp"
#include "LeaveMsg.hpp"
#include "NackMsg.hpp"
#include "NewChildMsg.hpp"
#include "NewFatherMsg.hpp"
#include "NewStrNodeMsg.hpp"
#include "RollbackMsg.hpp"
#include "StrNodeNeededMsg.hpp"
#include "TransactionMsg.hpp"
#include "UpdateZoneMsg.hpp"

REGISTER_MESSAGE(AckMsg);
REGISTER_MESSAGE(CommitMsg);
REGISTER_MESSAGE(InitStructNodeMsg);
REGISTER_MESSAGE(InsertCommandMsg);
REGISTER_MESSAGE(InsertMsg);
REGISTER_MESSAGE(LeaveMsg);
REGISTER_MESSAGE(NackMsg);
REGISTER_MESSAGE(NewChildMsg);
REGISTER_MESSAGE(NewFatherMsg);
REGISTER_MESSAGE(NewStrNodeMsg);
REGISTER_MESSAGE(RollbackMsg);
REGISTER_MESSAGE(StrNodeNeededMsg);
REGISTER_MESSAGE(TransactionMsg);
REGISTER_MESSAGE(UpdateZoneMsg);
