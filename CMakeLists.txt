cmake_minimum_required(VERSION 2.6)
add_subdirectory(cmake)

# STaRS, Scalable Task Routing approach to distributed Scheduling
project(stars)

# Generate test cases for CTest
enable_testing()

# Check dependencies
find_package(Boost 1.46 REQUIRED date_time filesystem program_options system thread unit_test_framework iostreams signals)
find_package(Sqlite3 REQUIRED)
find_package(Log4CPP REQUIRED)
find_package(MsgPack REQUIRED)
find_package(Wt)

# The version number.
set (STaRS_VERSION_MAJOR 0)
set (STaRS_VERSION_MINOR 1-alpha)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${PROJECT_SOURCE_DIR}/include/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)

# add the binary tree to the search path for include files
# so that we will find config.h
include_directories("${PROJECT_BINARY_DIR}")

message(STATUS "Building STaRS version " ${STaRS_VERSION_MAJOR} "." ${STaRS_VERSION_MINOR} " build " ${CMAKE_BUILD_TYPE})

if (NOT CMAKE_BUILD_TYPE STREQUAL Release)
    set (BIN_SUFFIX -${CMAKE_BUILD_TYPE})
endif (NOT CMAKE_BUILD_TYPE STREQUAL Release)

# Common names
set(STARS_LIBRARIES stars${BIN_SUFFIX})
set(STARS_DB_LIBRARIES stars_db${BIN_SUFFIX})
# Web UI library
if (Wt_FOUND)
    set (STARS_UI_LIBRARIES stars_ui${BIN_SUFFIX})
endif (Wt_FOUND)


add_subdirectory(src)
