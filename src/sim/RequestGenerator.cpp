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

#include <sstream>
#include <vector>
#include <stdexcept>
#include "RequestGenerator.hpp"
#include "TaskDescription.hpp"
#include "DispatchCommandMsg.hpp"
#include "Simulator.hpp"
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;


void RequestGenerator::getValues(std::vector<double> & v, const std::string & values) {
    for (size_t firstpos = 0, lastpos = 0; lastpos != std::string::npos; firstpos = lastpos + 1) {
        lastpos = values.find_first_of(';', firstpos);
        double value;
        std::istringstream(values.substr(firstpos, lastpos - firstpos)) >> value;
        v.push_back(value);
    }
}


void RequestGenerator::createUniformCDF(CDF & cdf, const std::string & values) {
    std::vector<double> v;
    getValues(v, values);
    if (v.size() == 1) {
        cdf.addValue(v[0], 1.0);
    } else if (v.size() > 1) {
        double resolution = 1.0 / v.size();
        double prob = 0.0;
        for (unsigned int i = 0; i < v.size(); i++)
            cdf.addValue(v[i], prob += resolution);
        cdf.addValue(v.back(), 1.0);
    }
}


RequestGenerator::RequestGenerator(const Properties & property) {
    fs::path appFile(property("app_distribution", std::string("")));
    if (fs::exists(appFile)) {
        Histogram adhist(1.0);
        // Load app class distribution
        fs::ifstream ifs(appFile);
        std::string nextLine;
        unsigned int nextDescription = 0;
        // skip first line
        getline(ifs, nextLine);
        while (ifs.good()) {
            getline(ifs, nextLine);
            // Skip comments
            if (nextLine[0] == '#') continue;
            SWFAppDescription ad;
            unsigned int frequency;
            std::istringstream(nextLine) >> ad.length >> ad.numTasks >> ad.deadline >> ad.maxMemory >> frequency;
            descriptions.push_back(ad);
            for (unsigned int i = 0; i < frequency; i++)
                adhist.addValue(nextDescription);
            nextDescription++;
        }
        appDistribution.loadFrom(adhist);
    } else {
        // Create an uniform set of applications with task_length, request_size and task_deadline properties
        std::vector<double> taskLength, requestSize, taskDeadline;
        getValues(taskLength, property("task_length", std::string("240000;2400000;10000000")));
        getValues(requestSize, property("request_size", std::string("5;10;20")));
        getValues(taskDeadline, property("task_deadline", std::string("1.3")));
        for (std::vector<double>::iterator i = taskLength.begin(); i != taskLength.end(); i++)
            for (std::vector<double>::iterator j = requestSize.begin(); j != requestSize.end(); j++)
                for (std::vector<double>::iterator k = taskDeadline.begin(); k != taskDeadline.end(); k++)
                    descriptions.push_back(SWFAppDescription(*i, *j, *k, -1));
        double resolution = 1.0 / descriptions.size();
        double prob = 0.0;
        for (unsigned int i = 0; i < descriptions.size(); i++)
            appDistribution.addValue(i, prob += resolution);
        appDistribution.addValue(descriptions.size(), 1.0);
    }
    // Load max mem and disk distributions
    std::string mem_values = property("task_max_mem", std::string("1024")),
            disk_values = property("task_max_disk", std::string("1024"));
    if (fs::exists(mem_values))
        taskMemory.loadFrom(mem_values);
    else createUniformCDF(taskMemory, mem_values);
    if (fs::exists(disk_values))
        taskDisk.loadFrom(disk_values);
    else createUniformCDF(taskDisk, disk_values);
    input = property("task_input_size", 0);
    output = property("task_output_size", 0);
}


DispatchCommandMsg * RequestGenerator::generate(StarsNode & client, Time releaseDate) {
    DispatchCommandMsg * dcm = new DispatchCommandMsg;
    // Create application
    TaskDescription minReq;
    SWFAppDescription & ad = descriptions[floor(appDistribution.inverse(Simulator::uniform01()))];
    minReq.setMaxMemory(ad.maxMemory == -1 ? taskMemory.inverse(Simulator::uniform01()) : ad.maxMemory / 1024);
    minReq.setMaxDisk(taskDisk.inverse(Simulator::uniform01()));
    minReq.setNumTasks(ad.numTasks);
    minReq.setLength(ad.length);
    minReq.setInputSize(input);
    minReq.setOutputSize(output);
    client.getDatabase().setNextApp(minReq);

    // Create instance
    //double d = minReq.getLength() * ad.deadline / client.getAveragePower();
    double d = ad.deadline;
    if (d <= 0.0 || d > 31536000.0)
        d = 31536000.0;
    dcm->setDeadline(releaseDate + Duration(d));
    return dcm;
}
