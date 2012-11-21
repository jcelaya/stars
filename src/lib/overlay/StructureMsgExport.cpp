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
