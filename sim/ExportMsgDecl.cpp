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

#ifdef _STATIC_

#include "BoostSerializationArchiveExport.hpp"
#include "BasicMsg.hpp"
#include "TaskEventMsg.hpp"
#include "TaskStateChgMsg.hpp"
#include "AvailabilityInformation.hpp"
#include "BasicAvailabilityInfo.hpp"
#include "QueueBalancingInfo.hpp"
#include "TimeConstraintInfo.hpp"
#include "StretchInformation.hpp"
#include "TaskBagMsg.hpp"
#include "UpdateTimer.hpp"
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
#include "DispatchCommandMsg.hpp"
#include "AcceptTaskMsg.hpp"
#include "AbortTaskMsg.hpp"
#include "TaskMonitorMsg.hpp"
#include "Logger.hpp"


BOOST_CLASS_EXPORT(BasicMsg);
BOOST_CLASS_EXPORT(TaskEventMsg);
BOOST_CLASS_EXPORT(TaskStateChgMsg);
BOOST_CLASS_EXPORT(AvailabilityInformation);
BOOST_CLASS_EXPORT(BasicAvailabilityInfo);
BOOST_CLASS_EXPORT(TimeConstraintInfo);
BOOST_CLASS_EXPORT(QueueBalancingInfo);
BOOST_CLASS_EXPORT(StretchInformation);
BOOST_CLASS_EXPORT(TaskBagMsg);
BOOST_CLASS_EXPORT(UpdateTimer);
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
BOOST_CLASS_EXPORT(DispatchCommandMsg);
BOOST_CLASS_EXPORT(AcceptTaskMsg);
BOOST_CLASS_EXPORT(AbortTaskMsg);
BOOST_CLASS_EXPORT(TaskMonitorMsg);

// Log facilities
BOOST_CLASS_EXPORT(LogMsg::AbstractTypeContainer);
BOOST_CLASS_EXPORT(LogMsg::ConstStringReference);
BOOST_CLASS_EXPORT(LogMsg::StringContainer);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<std::string>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<int8_t>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<int32_t>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<int64_t>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<uint8_t>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<uint32_t>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<uint64_t>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<float>);
BOOST_CLASS_EXPORT(LogMsg::TypeReference<double>);

#endif // _STATIC_
