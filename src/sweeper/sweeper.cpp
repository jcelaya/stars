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
#include <iostream>
#include <fstream>
#include <list>
#include <map>
using namespace std;


void getPropertiesList(const string & fileName, std::list<std::map<std::string, std::string> > & combinations);


int main(int argc, char * argv[]) {
    if (argc != 2) {
        cout << "Usage: sweeper config_file" << endl;
        return 1;
    }
    string configFile(argv[1]);
    list<map<string, string> > caseInstances;
    getPropertiesList(configFile, caseInstances);
    int caseNum = 0, totalSims = caseInstances.size();
    cout << "Generating " << totalSims << " simulation cases." << endl;
    while (!caseInstances.empty()) {
        map<string, string> & instance = caseInstances.front();
        // Create config file
        ostringstream oss;
        oss << configFile << '.' << caseNum++;
        ofstream ofs(oss.str().c_str());
        for (map<string, string>::const_iterator it = instance.begin(); it != instance.end(); it++) {
            ofs << it->first << "=" << it->second << endl;
        }
        caseInstances.pop_front();
    }
    return 0;
}
