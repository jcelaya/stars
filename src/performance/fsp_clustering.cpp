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

#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <log4cpp/Category.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/OstreamAppender.hh>
#include "FSPAvailabilityInformation.hpp"
#include "Logger.hpp"
using namespace std;
using namespace stars;
using namespace boost::posix_time;
using namespace log4cpp;

int main(int argc, char * argv[]) {
    Logger::initLog("Ex.RI.Aggr.FSP=INFO");
    OstreamAppender * console = new OstreamAppender("ConsoleAppender", &std::cout);
    PatternLayout * l = new PatternLayout();
    l->setConversionPattern("%d{%H:%M:%S.%l} %p %c : %m%n");
    console->setLayout(l);
    Category::getRoot().addAppender(console);

    if (argc != 5) {
        cout << "Usage: fsp-clustering clusters distvecsize pieces reducquality" << endl;
        return 1;
    }

    unsigned int clusters, distvecsize, pieces, reducquality;
    istringstream(argv[1]) >> clusters;
    istringstream(argv[2]) >> distvecsize;
    istringstream(argv[3]) >> pieces;
    istringstream(argv[4]) >> reducquality;
    stars::FSPAvailabilityInformation::setNumClusters(clusters);
    stars::ClusteringList<FSPAvailabilityInformation::MDZCluster>::setDistVectorSize(distvecsize);
    ZAFunction::setNumPieces(pieces);
    ZAFunction::setReductionQuality(reducquality);

    ifstream ifs("fsptest.dat");
    if (ifs) {
        msgpack::unpacker pac;
        ifs.seekg (0, ifs.end);
        int length = ifs.tellg();
        ifs.seekg (0, ifs.beg);
        pac.reserve_buffer(length);
        ifs.read(pac.buffer(), length);
        pac.buffer_consumed(length);
        FSPAvailabilityInformation * fspai = static_cast<FSPAvailabilityInformation *>(BasicMsg::unpackMessage(pac));
        long int numClusters = fspai->getSummary().size();
        cout << "Reducing " << numClusters << " cluster: ";
        ptime start = microsec_clock::local_time();
        fspai->reduce();
        ptime end = microsec_clock::local_time();
        long mus = (end - start).total_microseconds();
        cout << mus << " us" << endl;
    }
}
