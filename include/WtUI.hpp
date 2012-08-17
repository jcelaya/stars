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


#ifndef WTUI_H_
#define WTUI_H_

#include <boost/scoped_ptr.hpp>
#include <Wt/WServer>


/**
 * \brief Web-based user interface with WebToolkit
 *
 * This class is a singleton that initiates a web server to implement a web-based user interface.
 * This is done through the WebToolkit library (http://www.webtoolkit.eu/wt).
 */
class WtUI {
    boost::scoped_ptr<Wt::WServer> serverInstance;

    // Avoid instantiation
    WtUI() {}

public:
    static WtUI & getInstance() {
        static WtUI instance;
        return instance;
    }

    ~WtUI() {
        stop();
    }

    /**
     * Sets up the user interface
     */
    void setup();

    /**
     * Starts the server in the background
     */
    void start();

    /**
     * Stops the server
     */
    void stop();
};

#endif /* WTUI_H_ */
