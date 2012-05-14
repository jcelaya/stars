/*
 * RequestGenerator.h
 *
 *  Created on: 04/03/2010
 *      Author: javi
 */

#ifndef REQUESTGENERATOR_H_
#define REQUESTGENERATOR_H_

#include <string>
#include <vector>
#include "../Properties.hpp"
#include "../Distributions.hpp"
#include "../PeerCompNode.hpp"
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

    boost::shared_ptr<DispatchCommandMsg> generate(PeerCompNode & client, Time releaseDate);
};

#endif /* REQUESTGENERATOR_H_ */
