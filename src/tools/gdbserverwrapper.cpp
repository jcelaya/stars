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

#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <errno.h>
using namespace std;

int main(int argc, char * argv[]) {
    struct sockaddr_in localAddress;
    struct sockaddr * p = (struct sockaddr *) &localAddress;
    size_t size = sizeof(localAddress);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if( sockfd < 0 ) {
        cout << "socket error" << endl;
        return 1;
    }

    int port = 1024;
    int result;

    bzero((char *) &localAddress, sizeof(localAddress));
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = INADDR_ANY;
    localAddress.sin_port = htons(port);
    while ((result = bind(sockfd, p, size)) < 0 && errno == EADDRINUSE) {
        ++port;
        localAddress.sin_port = htons(port);
    }
    if(result < 0) {
        cerr << "Could not bind to process: (" << errno << ") " << strerror(errno) << endl;
        close (sockfd);
    } else {
        close (sockfd);
        ostringstream portNumber, pidNumber;
        portNumber << "host:" << port;
        pidNumber << getppid();
        execl("/usr/bin/gdbserver", "/usr/bin/gdbserver", portNumber.str().c_str(), "--attach", pidNumber.str().c_str(), NULL);
    }
    return 1;
}
