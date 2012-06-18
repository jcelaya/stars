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

#ifndef TASKDESCRIPTION_H_
#define TASKDESCRIPTION_H_

#include "Time.hpp"
#include "CommAddress.hpp"
//#include <string>


/**
 * \brief Description of a task requirements.
 *
 * This class just implements objects that describe the resource requirements of a task.
 */
class TaskDescription {
public:
    /// Default constructor
    TaskDescription() : length(0), numTasks(0), maxMemory(0), maxDisk(0), inputSize(0), outputSize(0) {}

    // Getters and Setters

    /**
     * Returns the length of the task
     */
    uint64_t getLength() const {
        return length;
    }

    /**
     * Sets the length of the task
     */
    void setLength(uint64_t l) {
        length = l;
    }

    /**
     * Returns the number of tasks contained in this request.
     * @return Number of tasks.
     */
    uint32_t getNumTasks() const {
        return numTasks;
    }

    /**
     * Sets the number of tasks that is requested to be assigned.
     * @param n Number of tasks.
     */
    void setNumTasks(uint32_t n) {
        numTasks = n;
    }

    /**
     * Returns the length of the whole app
     */
    uint64_t getAppLength() const {
        return length * numTasks;
    }

    /**
     * Returns the deadline of the task.
     */
    Time getDeadline() const {
        return deadline;
    }

    /**
     * Sets the deadline of the task.
     */
    void setDeadline(Time d) {
        deadline = d;
    }

    /**
     * Returns the maximum memory usage of the task.
     */
    uint32_t getMaxMemory() const {
        return maxMemory;
    }

    /**
     * Sets the maximum memory usage of the task.
     */
    void setMaxMemory(uint32_t m) {
        maxMemory = m;
    }

    /**
     * Returns the maximum disk usage of the task.
     */
    uint32_t getMaxDisk() const {
        return maxDisk;
    }

    /**
     * Sets the maximum disk usage of the task.
     */
    void setMaxDisk(uint32_t d) {
        maxDisk = d;
    }

    /**
     * Returns the size of the input data
     */
    uint32_t getInputSize() const {
        return inputSize;
    }

    /**
     * Sets the size of the input data
     */
    void setInputSize(uint32_t i) {
        inputSize = i;
    }

    /**
     * Returns the size of the ou8tput data
     */
    uint32_t getOutputSize() const {
        return outputSize;
    }

    /**
     * Sets the size of the output data
     */
    void setOutputSize(uint32_t o) {
        outputSize = o;
    }
    
    MSGPACK_DEFINE(length, numTasks, deadline, maxMemory, maxDisk, inputSize, outputSize);
private:
    uint64_t length;       ///< Length in millions of instructions
    uint32_t numTasks;     ///< Number of tasks in the application
    Time deadline;        ///< Absolute deadline
    uint32_t maxMemory;    ///< Maximum memory used, in kilobytes
    uint32_t maxDisk;      ///< Maximum disk space used, in kilobytes
    uint32_t inputSize;    ///< Input data size, in kilobytes
    uint32_t outputSize;   ///< Output data size, in kilobytes
    //string inputURL;
    //string outputURL;
};

#endif /*TASKDESCRIPTION_H_*/
