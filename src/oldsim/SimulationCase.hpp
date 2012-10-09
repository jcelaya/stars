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

#ifndef SIMULATIONCASE_H_
#define SIMULATIONCASE_H_

#include <string>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>
#include "Properties.hpp"
class BasicMsg;

class SimulationCase {
protected:
    Properties property;
    double percent;

    // During SimulationCase constructor, the simulator is not initialized
    SimulationCase(const Properties & p) : property(p), percent(0.0) {}

public:
    virtual ~SimulationCase() {}

    virtual void preStart() {}
    virtual void postEnd() {}
    virtual bool doContinue() const { return true; }
    virtual void beforeEvent(uint32_t src, uint32_t dst, const BasicMsg & msg) {}
    virtual void afterEvent(uint32_t src, uint32_t dst, const BasicMsg & msg) {}
    double getCompletedPercent() const { return percent; }
    // Some common events
    virtual void finishedApp(long int appId) {}
};


class CaseFactory {
    std::map<std::string, boost::shared_ptr<SimulationCase> (*)(const Properties & p)> caseConstructors;

    CaseFactory() {}

public:
    static CaseFactory & getInstance() {
        static CaseFactory s;
        return s;
    }

    template<class S> class CaseRegistrar {
        static boost::shared_ptr<SimulationCase> create(const Properties & p) {
            return boost::shared_ptr<SimulationCase>(new S(p));
        }
    public:
        CaseRegistrar() {
            CaseFactory::getInstance().caseConstructors[S::getName()] = &create;
        }
    };
    template<class S> friend class CaseRegistrar;

    boost::shared_ptr<SimulationCase> createCase(const std::string & name, const Properties & p);
};


#define REGISTER_SIMULATION_CASE(name) CaseFactory::CaseRegistrar<name> BOOST_JOIN(name, _reg_var)


#endif /*SIMULATIONCASE_H_*/
