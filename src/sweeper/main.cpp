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

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <sstream>
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace std;
using namespace boost::posix_time;


unsigned long int getAvailableMemory() {
    unsigned long int free;
    unsigned long int total;
    string dummy;
    ifstream file("/proc/meminfo");
    file >> dummy >> total;
    file.ignore(1000, '\n');
    file >> dummy >> free;
    return (free - total / 10) >> 10;   // Approx 90% of total memory
}


class Simulations {
    list<map<string, string> > caseInstances;
    bool end;
    unsigned int numProcesses;
    unsigned long int availableMemory;
    list<pair<pid_t, unsigned long int> > processes;
    string simExec, configFile;

    Simulations() : end(false), numProcesses(1), availableMemory(getAvailableMemory()), configFile("simulations.conf") {}

public:
    static Simulations & getInstance() {
        static Simulations s;
        return s;
    }

    void parseCmdLine(int argc, char * argv[]);
    int run();
    void stop();
};


void getPropertiesList(const string & fileName, std::list<std::map<std::string, std::string> > & combinations);


void finish(int param) {
    Simulations::getInstance().stop();
}


int main(int argc, char * argv[]) {
    Simulations & sim = Simulations::getInstance();
    sim.parseCmdLine(argc, argv);
    signal(SIGTERM, finish);
    return sim.run();
}


void Simulations::parseCmdLine(int argc, char * argv[]) {
    for (int i = 1; i < argc; i++) {
        if (argv[i] == string("-c") && ++i < argc)
            configFile = argv[i];
        else if (argv[i] == string("-e") && ++i < argc)
            simExec = argv[i];
        else if (argv[i] == string("-p") && ++i < argc)
            istringstream(argv[i]) >> numProcesses;
        else if (argv[i] == string("-m") && ++i < argc)
            istringstream(argv[i]) >> availableMemory;
    }
    getPropertiesList(configFile, caseInstances);
}


int Simulations::run() {
    ptime start = microsec_clock::local_time();
    bool dryRun = simExec == "";
    int caseNum = 0, totalSims = caseInstances.size();
    cout << "Starting simulations at " << start << endl;
    cout << "Generating " << totalSims << " simulation cases." << endl;
    if (!dryRun)
        cout << "Using " << numProcesses << " processors and " << availableMemory << " megabytes of memory." << endl;
    while (!caseInstances.empty()) {
        map<string, string> & instance = caseInstances.front();
        unsigned long int mem = 0;
        map<string, string>::iterator it = instance.find("max_mem");
        if (it != instance.end())
            istringstream(it->second) >> mem;
        pid_t pid;
        // Wait until there are enough free processes
        while (!processes.empty() && (processes.size() == numProcesses || mem > availableMemory)) {
            pid = wait(NULL);
            for (list<pair<pid_t, unsigned long int> >::iterator it = processes.begin(); it != processes.end(); it++)
                if (it->first == pid) {
                    availableMemory += it->second;
                    processes.erase(it);
                    break;
                }
        }
        if (end) break;
        if (processes.empty() && mem > availableMemory) {
            cout << "Unable to run simulation " << caseNum++ << ", not enough memory." << endl;
            caseInstances.pop_front();
            continue;
        }
        // Then spawn a new one
        // Create config file
        ostringstream oss;
        oss << configFile << '.' << caseNum++;
        ofstream ofs(oss.str().c_str());
        for (map<string, string>::const_iterator it = instance.begin(); it != instance.end(); it++) {
            ofs << it->first << "=" << it->second << endl;
        }
        if (!dryRun) {
            if ((pid = fork())) {
                availableMemory -= mem;
                processes.push_back(make_pair(pid, mem));
            } else {
                cout << "Starting simulation " << caseNum << " out of " << totalSims;
                execl(simExec.c_str(), simExec.c_str(), oss.str().c_str(), NULL);
                // This function only returns on error
                cout << "Error running " << simExec << oss.str() << endl;
                return 1;
            }
        }
        caseInstances.pop_front();
    }
    // Wait for all the processes to finish
    for (unsigned int i = 0; i < processes.size(); i++) wait(NULL);
    ptime end = microsec_clock::local_time();
    cout << "Finished at " << end << ", lasted " << (end - start) << endl;
    return 0;
}


void Simulations::stop() {
    end = true;
    cout << "Stopping due to user signal" << endl;
    for (list<pair<pid_t, unsigned long int> >::iterator it = processes.begin(); it != processes.end(); it++)
        kill(it->first, SIGTERM);
}
