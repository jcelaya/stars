include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIR})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(../include)
include_directories(.)

add_subdirectory(cases)

# Sources
set(oldsim_sources
    AvailabilityStatistics.cpp
    Distributions.cpp
    FailureGenerator.cpp
    MemoryManager.cpp
    StarsNode.cpp
    LibStarsStatistics.cpp
    PerfectScheduler.cpp
    PerformanceStatistics.cpp
    Properties.cpp
    SimAppDatabase.cpp
    SimTask.cpp
    SimulationCase.cpp
    Simulator.cpp
    TrafficStatistics.cpp
    ${cases_sources})

set(STARS_LIBRARIES stars${BIN_SUFFIX})

# add the executable
add_executable(stars-oldsim${BIN_SUFFIX} ${oldsim_sources})
target_link_libraries(stars-oldsim${BIN_SUFFIX} ${STARS_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES} ${MSGPACK_LIBRARIES})
