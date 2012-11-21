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
#include <Wt/WApplication>
#include <Wt/WBreak>
#include <Wt/WContainerWidget>
#include <Wt/WLineEdit>
#include <Wt/WPushButton>
#include <Wt/WText>
#include "WtUI.hpp"
#include "ConfigurationManager.hpp"
#include "Logger.hpp"
using namespace Wt;
using namespace std;
using namespace boost;


class InterfaceApp : public WApplication {
    WLineEdit * nameEdit;
    WText * greeting;

    void greet() {
        greeting->setText("Hello there, " + nameEdit->text());
    }

public:
    InterfaceApp(const WEnvironment & env);

    /**
     * Create a new session
     */
    static WApplication * createApplication(const WEnvironment & env) {
        return new InterfaceApp(env);
    }
};


InterfaceApp::InterfaceApp(const WEnvironment & env) : WApplication(env) {
    setTitle("STaRS Web Interface");                               // application title

    root()->addWidget(new WText("Your name, please ? "));  // show some text
    nameEdit = new WLineEdit(root());                     // allow text input
    nameEdit->setFocus();                                 // give focus

    WPushButton * b = new WPushButton("Greet me.", root()); // create a button
    b->setMargin(5, Left);                                 // add 5 pixels margin

    root()->addWidget(new WBreak());                       // insert a line break

    greeting = new WText(root());                         // empty text

    /*
    * Connect signals with slots
    *
    * - simple Wt-way
    */
    b->clicked().connect(this, &InterfaceApp::greet);

    /*
    * - using an arbitrary function object (binding values with boost::bind())
    */
    nameEdit->enterPressed().connect(boost::bind(&InterfaceApp::greet, this));
}


void WtUI::setup() {
    // Load fixed options, only possible through argc/argv pair
    string HttpPortStr = static_cast<ostringstream &>(ostringstream() << ConfigurationManager::getInstance().getUIPort()).str();
    string docRoot = (ConfigurationManager::getInstance().getWorkingPath() / "ui_files").string();
    const char * argv[] = {
        "STaRS",
        "--docroot", docRoot.c_str(),
        "--http-address", "0.0.0.0",
        "--http-port", HttpPortStr.c_str(),
//   "--https-address", "",
//   "--https-port", "",
//   "--ssl-certificate", "",
//   "--ssl-private-key", "",
//   "--ssl-tmp-dh", "",
    };
    int argc = sizeof(argv) / sizeof(const char *);
    serverInstance.reset(new WServer(string("STaRS"), (ConfigurationManager::getInstance().getWorkingPath() / "ui.xml").string()));
    serverInstance->setServerConfiguration(argc, const_cast<char **>(argv));
    serverInstance->addEntryPoint(Wt::Application, InterfaceApp::createApplication);
}


void WtUI::start() {
    if (!serverInstance.get()) setup();

    if (!serverInstance->start()) {
        LogMsg("UI", ERROR) << "Unable to start UI web server";
    }
}


void WtUI::stop() {
    if (serverInstance.get())
        serverInstance->stop();
}
