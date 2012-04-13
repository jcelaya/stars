cmake_minimum_required(VERSION 2.6)

# STaRS, Scalable Task Routing approach to distributed Scheduling
project(stars)

# Check dependencies
find_package(Boost 1.46 REQUIRED date_time filesystem program_options system)

# The version number.
set (STaRS_VERSION_MAJOR 0)
set (STaRS_VERSION_MINOR 1-alpha)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)
 
# add the binary tree to the search path for include files
# so that we will find config.h
include_directories("${PROJECT_BINARY_DIR}")

add_subdirectory(src)
add_subdirectory(test)