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

#ifndef REQUESTGENERATOR_H_
#define REQUESTGENERATOR_H_

#include <string>
#include <vector>
#include "../Properties.hpp"
#include "../Distributions.hpp"
#include "../StarsNode.hpp"
#include "DispatchCommandMsg.hpp"
#include "Time.hpp"

class RequestGenerator {
    struct SWFAppDescription {
        unsigned int length;       ///< Length in millions of instructions
        unsigned int numTasks;     ///< Number of tasks in the application
        double deadline;           ///< Relative deadline
        int maxMemory;             ///< Maximum memory used, in kilobytes
        SWFAppDescription() {}
        SWFAppDescription(unsigned int l, unsigned int n, double d, int m) : length(l), numTasks(n), deadline(d), maxMemory(m) {}
    };
    std::vector<SWFAppDescription> descriptions;
    CDF appDistribution;

    CDF taskMemory;
    CDF taskDisk;
    unsigned int input, output;

    void createUniformCDF(CDF & v, const std::string & values);
    void getValues(std::vector<double> & v, const std::string & values);

public:
    RequestGenerator(const Properties & property);

    boost::shared_ptr<DispatchCommandMsg> generate(StarsNode & client, Time releaseDate);
};

#endif /* REQUESTGENERATOR_H_ */
