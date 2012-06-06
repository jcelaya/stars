/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
 *
 *  This file is part of PeerComp.
 *
 *  PeerComp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  PeerComp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PeerComp; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef CONFIGURATIONMANAGER_H_
#define CONFIGURATIONMANAGER_H_

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include "CommAddress.hpp"


/**
 * Provides different configuration parameters that can be set by the user through the GUI
 * or through a configuration file.
 */
class ConfigurationManager {
    /// Options description
    boost::program_options::options_description description;

    /// Working directory of the application.
    boost::filesystem::path workingPath;
    double updateBW;          ///< Update max bandwidth for availability information
    double slownessRatio;     ///< Maximum relation between maximum and minimum slowness
    uint16_t port;            ///< TCP port to listen to
    uint16_t uiPort;          ///< TCP port for UI connections
    std::string logString;    ///< Logging configuration string
    int submitRetries;        ///< Number of retries of a failing submission
    unsigned int heartbeat;   ///< Number of seconds between heartbeat signals from scheduler to submission nodes
    unsigned int availMemory;       ///< Available memory for tasks
    unsigned int availDisk;         ///< Available disk for tasks
    boost::filesystem::path dbPath;
    std::string entryPoint;

    /// default constructor, prevents instantiation
    ConfigurationManager();

public:

    /**
     * Provides the entry point for the singleton pattern.
     * @returns The singleton instance.
     */
    static ConfigurationManager & getInstance();

    /**
     * Loads configuration from a specific file
     */
    void loadConfigFile(boost::filesystem::path configFile);

    /**
     * Loads configuration from command line
     * @returns True if the program should exit, due to options like "help", "version", etc...
     */
    bool loadCommandLine(int argc, char * argv[]);

    // Getters and Setters

    /**
     * Returns the working path of the application. It is used for relative paths.
     */
    boost::filesystem::path getWorkingPath() {
        return workingPath;
    }

    /**
     * Sets the working path of the application.
     */
    void setWorkingPath(boost::filesystem::path p) {
        workingPath = p;
    }

    /**
     * Returns the maximum bandwidth for availability information updates.
     */
    double getUpdateBandwidth() const {
        return updateBW;
    }

    /**
     * Sets the maximum bandwidth for availability information updates.
     */
    void setUpdateBandwidth(double bw) {
        updateBW = bw;
    }

    /**
     * Returns the maximum relation between maximum and minimum slowness.
     */
    double getSlownessRatio() const {
        return slownessRatio;
    }

    /**
     * Sets the maximum relation between maximum and minimum slowness.
     */
    void setSlownessRatio(double sr) {
        slownessRatio = sr;
    }

    /**
     * Returns the port number.
     */
    uint16_t getPort() const {
        return port;
    }

    /**
     * Sets the port number.
     */
    void setPort(uint16_t p) {
        port = p;
    }

    /**
     * Returns the port number for the user interface.
     */
    uint16_t getUIPort() const {
        return uiPort;
    }

    /**
     * Sets the port number for the user interface.
     */
    void setUIPort(uint16_t p) {
        uiPort = p;
    }

    /**
     * Returns the logging configuration string.
     */
    const std::string & getLogConfig() const {
        return logString;
    }

    /**
     * Sets the logging configuration string.
     */
    void setLogConfig(const std::string & s) {
        logString = s;
    }

    /**
     * Returns the number of retries of a failed submission.
     */
    int getSubmitRetries() const {
        return submitRetries;
    }

    /**
     * Sets the number of retries of a failed submission.
     */
    void setSubmitRetries(int r) {
        submitRetries = r;
    }

    /**
     * Returns the number of seconds between heartbeat signals from scheduler to submission nodes
     */
    unsigned int getHeartbeat() const {
        return heartbeat;
    }

    /**
     * Sets the number of seconds between heartbeat signals from scheduler to submission nodes
     */
    void setHeartbeat(unsigned int h) {
        heartbeat = h;
    }

    /**
     * Returns the working path of the application. It is used for relative paths.
     */
    boost::filesystem::path getDatabasePath() {
        return dbPath;
    }

    /**
     * Sets the working path of the application.
     */
    void setDatabasePath(boost::filesystem::path p) {
        dbPath = p;
    }

    /**
     * Returns the maximum relation between maximum and minimum stretch.
     */
    unsigned int getAvailableMemory() const {
        return availMemory;
    }

    /**
     * Sets the maximum relation between maximum and minimum stretch.
     */
    void setAvailableMemory(unsigned int m) {
        availMemory = m;
    }

    /**
     * Returns the maximum relation between maximum and minimum stretch.
     */
    unsigned int getAvailableDisk() const {
        return availDisk;
    }

    /**
     * Sets the maximum relation between maximum and minimum stretch.
     */
    void setAvailableDisk(unsigned int d) {
        availDisk = d;
    }

    CommAddress getEntryPoint() const;
};

#endif /* CONFIGURATIONMANAGER_H_ */
