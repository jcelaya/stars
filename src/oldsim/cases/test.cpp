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

#include <unistd.h>
#include "SimulationCase.hpp"


class noop : public SimulationCase {

public:
    noop(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }

    static const std::string getName() { return std::string("noop"); }

    void preStart() {
        int w = property("wait", 5);
        sleep(w);
    }

    bool doContinue() const {
        return false;
    }
};
REGISTER_SIMULATION_CASE(noop);
