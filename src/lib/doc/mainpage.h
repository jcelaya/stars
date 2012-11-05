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

/**
 * \mainpage
 * \section intro Welcome to the STaRS design documentation.
 *
 * This document contains the design specifications of STaRS, from the architecture to the coding details.
 *
 * \section architecture Software Architecture.
 * STaRS software architecture is divided into several layers of functionality. The bottom layer is provided by the Core component, which implements basic I/O for other components. On top of this layer, the other components lay: Execution Manager, Global Scheduler, Structure Manager and Submission Manager. The Execution Manager controls the execution environment and the local scheduling. The Global Scheduler manages the availability information and the scheduling policies. The Structure Manager implements the network overlay construction and maintenance. Finally, the Submission Manager monitors the submission and remote execution of user applications. These components are described in detail in their own sections.
 */
