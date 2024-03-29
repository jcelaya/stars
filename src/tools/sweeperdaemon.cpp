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
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <sstream>
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
using namespace std;
using namespace boost;
namespace fs = boost::filesystem;


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
public:
    static Simulations & getInstance() {
        static Simulations s;
        return s;
    }

    void parseCmdLine(int argc, char * argv[]);
    int run(int argc, char * argv[]);
    void stop();
    void getNewCases();
    void waitProcesses();

    pid_t spawnProcess(const map<string, string> & properties);
    unsigned long int getMemoryLimit(const map<string, string> & properties) const {
        unsigned long int mem = 0;
        auto it = properties.find("max_mem");
        if (it != properties.end())
            istringstream(it->second) >> mem;
        return mem;
    }
    void reschedule();

private:
    list<map<string, string> > caseInstances;
    bool end, waitOnPipe;
    unsigned int numProcesses;
    unsigned long int availableMemory;
    list<pair<pid_t, unsigned long int> > processes;
    string simExec, pipeName;
    thread pipeThread, waitThread;
    mutex m;            ///< Mutex to put and get queue messages.
    condition newCasesOrProcesses;     ///< Barrier to wait for a message
    condition children;     ///< Barrier to wait for children

    Simulations() : end(false), waitOnPipe(false), numProcesses(1), availableMemory(getAvailableMemory()), pipeName("sweeperpipe") {}
};


void getPropertiesList(const string & fileName, std::list<std::map<std::string, std::string> > & combinations);


void finish(int param) {
    Simulations::getInstance().stop();
}


int main(int argc, char * argv[]) {
    return Simulations::getInstance().run(argc, argv);
}


void Simulations::parseCmdLine(int argc, char * argv[]) {
    for (int i = 1; i < argc; i++) {
        if (argv[i] == string("-f") && ++i < argc)
            pipeName = argv[i];
        else if (argv[i] == string("-e") && ++i < argc)
            simExec = argv[i];
        else if (argv[i] == string("-p") && ++i < argc)
            istringstream(argv[i]) >> numProcesses;
        else if (argv[i] == string("-m") && ++i < argc)
            istringstream(argv[i]) >> availableMemory;
    }
}


void Simulations::getNewCases() {
    while (true) {
        list<map<string, string> > newInstances;
        getPropertiesList(pipeName, newInstances);
        this_thread::interruption_point();
        if (newInstances.empty()) continue;
        cout << "Adding " << newInstances.size() << " more cases." << endl;
        {
            mutex::scoped_lock lock(m);
            caseInstances.splice(caseInstances.end(), newInstances);
        }
        newCasesOrProcesses.notify_all();
    }
}


void Simulations::waitProcesses() {
    while (true) {
        {
            mutex::scoped_lock lock(m);
            if (processes.empty()) {
                if (end)
                    return;
                children.wait(lock);
                if (end)
                    return;
            }
        }
        pid_t pid = wait(NULL);
        if (pid != -1) {
            cout << "Process " << pid << " ended." << endl;
            {
                mutex::scoped_lock lock(m);
                for (auto it = processes.begin(); it != processes.end(); ++it) {
                    if (it->first == pid) {
                        availableMemory += it->second;
                        processes.erase(it);
                        break;
                    }
                }
            }
            newCasesOrProcesses.notify_all();
        }
    }
}


int Simulations::run(int argc, char * argv[]) {
    signal(SIGTERM, finish);
    signal(SIGINT, finish);

    parseCmdLine(argc, argv);
    if (simExec == "") {
        cerr << "Usage: " << argv[0] << " -e sim_program [-f pipe_name] [-p num_processes] [-m max_memory]" << endl;
        return 1;
    }
    cout << "Using " << numProcesses << " processors and " << availableMemory << " megabytes of memory." << endl;

    if (fs::exists(pipeName) && fs::is_regular_file(pipeName)) {
        cout << "Reading configuration from " << pipeName << endl;
        getPropertiesList(pipeName, caseInstances);
        waitOnPipe = false;
    } else {
        mknod(pipeName.c_str(), S_IFIFO | 0600, 0);
        cout << "Listening on " << pipeName << endl;
        pipeThread = thread(bind(&Simulations::getNewCases, this));
        waitOnPipe = true;
    }

    waitThread = thread(bind(&Simulations::waitProcesses, this));

    do {
        mutex::scoped_lock lock(m);
        reschedule();
        if (!waitOnPipe && caseInstances.empty()) {
            end = true;
        } else {
            if (processes.empty() && waitOnPipe) {
                cout << "Waiting for tests..." << endl;
            }
            newCasesOrProcesses.wait(lock);
        }
    } while (!end);

    pipeThread.join();
    waitThread.join();
    return 0;
}


void Simulations::reschedule() {
    // Schedule as many cases as possible until memory is full
    auto instance = caseInstances.begin();
    while (processes.size() < numProcesses && instance != caseInstances.end()) {
        // Try to launch this case
        unsigned long int mem = getMemoryLimit(*instance);
        if (mem <= availableMemory) {
            pid_t pid = spawnProcess(*instance);
            availableMemory -= mem;
            processes.push_back(make_pair(pid, mem));
            children.notify_all();
            instance = caseInstances.erase(instance);
        }
        else {
            if (processes.empty()) {
                cout << "Unable to run simulation, not enough memory." << endl;
                instance = caseInstances.erase(instance);
            } else ++instance;
        }
    }
}


pid_t Simulations::spawnProcess(const map<string, string> & properties) {
    // Redirect stdio
    int fd[2];
    pipe(fd);
    pid_t pid;
    if ((pid = fork())) {
        // Close read end
        close(fd[0]);
        // Feed configuration through write end
        ostringstream oss;
        for (auto & i : properties) {
            oss << i.first << "=" << i.second << endl;
        }
        write(fd[1], oss.str().c_str(), oss.tellp());
        close(fd[1]);
        return pid;
    } else {
        // Close write end
        close(fd[1]);
        // Set read end as standard input
        dup2(fd[0], 0);
        execl(simExec.c_str(), simExec.c_str(), "-", NULL);
        // This function only returns on error
        cout << "Error running simulation." << endl;
        close(fd[0]);
        exit(1);
    }
}


void Simulations::stop() {
    end = true;
    pipeThread.interrupt();
    //waitThread.interrupt();
    cout << "Stopping current processes." << endl;
    for (list<pair<pid_t, unsigned long int> >::iterator it = processes.begin(); it != processes.end(); it++)
        kill(it->first, SIGTERM);
    children.notify_all();
    // Signal end of file in the pipe
    ofstream(pipeName.c_str(), ios::out | ios::app).close();
    newCasesOrProcesses.notify_all();
}
