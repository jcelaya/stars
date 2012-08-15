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

/**
 * \mainpage
 * \section intro Welcome to the PeerComp design documentation.
 *
 * This document contains the design specifications of PeerComp, from the architecture to the coding details.
 *
 * \section architecture Software Architecture.
 * PeerComp software architecture is divided into several layers of functionality. The bottom layer is provided by the Core component, which implements basic I/O for other components. On top of this layer, the other components lay: Execution Manager, Global Scheduler, Structure Manager and Submission Manager. The Execution Manager controls the execution environment and the local scheduling. The Global Scheduler manages the availability information and the scheduling policies. The Structure Manager implements the network overlay construction and maintenance. Finally, the Submission Manager monitors the submission and remote execution of user applications. These components are described in detail in their own sections.
 */
