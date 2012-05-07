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

#include "BoostSerializationArchiveExport.hpp"
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

BOOST_CLASS_EXPORT(AckMsg);
BOOST_CLASS_EXPORT(CommitMsg);
BOOST_CLASS_EXPORT(InitStructNodeMsg);
BOOST_CLASS_EXPORT(InsertCommandMsg);
BOOST_CLASS_EXPORT(InsertMsg);
BOOST_CLASS_EXPORT(LeaveMsg);
BOOST_CLASS_EXPORT(NackMsg);
BOOST_CLASS_EXPORT(NewChildMsg);
BOOST_CLASS_EXPORT(NewFatherMsg);
BOOST_CLASS_EXPORT(NewStrNodeMsg);
BOOST_CLASS_EXPORT(RollbackMsg);
BOOST_CLASS_EXPORT(StrNodeNeededMsg);
BOOST_CLASS_EXPORT(TransactionMsg);
BOOST_CLASS_EXPORT(UpdateZoneMsg);
