find_library(SIMGRID_LIBRARIES
    NAME simgrid
    HINTS
    $ENV{LD_LIBRARY_PATH}
    $ENV{GRAS_ROOT}
        $ENV{SIMGRID_ROOT}
    PATH_SUFFIXES lib64 lib
    PATHS
    /opt
    /opt/local
    /opt/csw
    /sw
    /usr
)

find_path(HAVE_MSG_H msg.h
    HINTS
    $ENV{GRAS_ROOT}
        $ENV{SIMGRID_ROOT}
    PATH_SUFFIXES include/msg
    PATHS
    /opt
    /opt/local
    /opt/csw
    /sw
    /usr
)

find_program(HAVE_TESH
        NAMES tesh
        HINTS
        $ENV{GRAS_ROOT}
        $ENV{SIMGRID_ROOT}
        PATH_SUFFIXES bin
        PATHS
        /opt
        /opt/local
        /opt/csw
        /sw
        /usr
)

set(SIMGRID_FOUND 0)
if(SIMGRID_LIBRARIES AND HAVE_MSG_H)
  set(SIMGRID_FOUND 1)
  get_filename_component(simgrid_version ${SIMGRID_LIBRARIES} REALPATH)
  string(REPLACE "${SIMGRID_LIBRARIES}." "" simgrid_version "${simgrid_version}")
  string(REGEX MATCH "^[0-9]" SIMGRID_MAJOR_VERSION "${simgrid_version}")
  string(REGEX MATCH "^[0-9].[0-9]" SIMGRID_MINOR_VERSION "${simgrid_version}")
  string(REGEX MATCH "^[0-9].[0-9].[0-9]" SIMGRID_PATCH_VERSION "${simgrid_version}")
  string(REGEX REPLACE "^${SIMGRID_MINOR_VERSION}." "" SIMGRID_PATCH_VERSION "${SIMGRID_PATCH_VERSION}") 
  string(REGEX REPLACE "^${SIMGRID_MAJOR_VERSION}." "" SIMGRID_MINOR_VERSION "${SIMGRID_MINOR_VERSION}")
  message(STATUS "Found SimGrid version : ${SIMGRID_MAJOR_VERSION}.${SIMGRID_MINOR_VERSION}")
else(SIMGRID_LIBRARIES AND HAVE_MSG_H)
  message(STATUS "Looking for lib SimGrid - not found")
endif(SIMGRID_LIBRARIES AND HAVE_MSG_H)
