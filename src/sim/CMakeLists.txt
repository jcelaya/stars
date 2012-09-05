find_package(SimGrid)

if(SIMGRID_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    include_directories(${SQLITE3_INCLUDE_DIR})
    include_directories(${LOG4CPP_INCLUDE_DIR})
    include_directories(../../include)
    include_directories(.)

    add_subdirectory(cases)

    # Sources
    set(starssim_sources
        Distributions.cpp
        MemoryManager.cpp
        StarsNode.cpp
        LibStarsStatistics.cpp
        PerformanceStatistics.cpp
        Properties.cpp
        SimAppDatabase.cpp
        SimTask.cpp
        SimulationCase.cpp
        Simulator.cpp
        RequestGenerator.cpp
        ${cases_sources})

    set(STARS_LIBRARIES stars${BIN_SUFFIX})

    # add the executable
    add_executable(stars-simgrid${BIN_SUFFIX} ${starssim_sources})
    target_link_libraries(stars-simgrid${BIN_SUFFIX} ${STARS_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES} ${SIMGRID_LIBRARIES} ${MSGPACK_LIBRARIES} ${Wt_LIBRARIES})
endif(SIMGRID_FOUND)
