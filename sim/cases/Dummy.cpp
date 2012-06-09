/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
 *
 *  This file is part of STaRS.
 *
 *  STaRS is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  STaRS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with STaRS; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../SimulationCase.hpp"


// Copy-Paste to create new test hehehe
class DummyTest : public SimulationCase {
public:
    DummyTest(const Properties & p) : SimulationCase(p) {
        // Prepare the properties
    }
    
    static const std::string getName() { return std::string("DummyTest"); }
    
    void preStart() {
        // Before running simulation
        // NOTE: outside MSG_main, do not call Time::getCurrentTime() or Simulator::getCurrentNode() !!
    }
    
    void postEnd() {
        // After ending simulation
        // NOTE: outside MSG_main, do not call Time::getCurrentTime() or Simulator::getCurrentNode() !!
    }
};
REGISTER_SIMULATION_CASE(DummyTest);
