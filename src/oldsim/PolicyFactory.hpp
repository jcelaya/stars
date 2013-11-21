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

#ifndef POLICYFACTORY_HPP_
#define POLICYFACTORY_HPP_

#include <memory>
#include "CommLayer.hpp"
#include "OverlayLeaf.hpp"
#include "OverlayBranch.hpp"
#include "MsgpackArchive.hpp"
class CentralizedScheduler;

class PolicyFactory {
public:
    static std::unique_ptr<PolicyFactory> getFactory(const std::string & scheduler, const std::string & policy);

    virtual ~PolicyFactory() {}

    virtual Service * createScheduler(OverlayLeaf & leaf) const = 0;
    virtual Service * createDispatcher(OverlayBranch & branch) const = 0;
    virtual std::shared_ptr<CentralizedScheduler> getCentScheduler() const = 0;
    virtual void serializeDispatcher(Service & disp, MsgpackOutArchive & ar) const = 0;
    virtual void serializeDispatcher(Service & disp, MsgpackInArchive & ar) const = 0;
    virtual void buildDispatcher(OverlayBranch & branch, Service & disp) const = 0;
    virtual void buildDispatcherDown(Service & disp, CommAddress localAddress) const = 0;

protected:
    PolicyFactory() {}
};

#endif /* POLICYFACTORY_HPP_ */
