find_package(SimGrid)

if(SIMGRID_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    include_directories(${SQLITE3_INCLUDE_DIR})
    include_directories(${LOG4CPP_INCLUDE_DIR})
    include_directories(../include)
    include_directories(.)

    # Sources
    file(GLOB starssim_sources *.cpp simulations/*.cpp)

    set(STARS_LIBRARIES stars-${CMAKE_BUILD_TYPE})

    # add the executable
    add_executable(stars-simgrid ${starssim_sources})
    target_link_libraries(stars-simgrid ${STARS_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES} ${SIMGRID_LIBRARIES})
endif(SIMGRID_FOUND)

