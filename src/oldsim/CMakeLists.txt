include_directories(${Boost_INCLUDE_DIRS})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(../../include)
include_directories(.)

add_subdirectory(cases)

# Sources
set(oldsim_sources
    ../util/MemoryManager.cpp
    ../util/SignalException.cpp
    AvailabilityStatistics.cpp
    Distributions.cpp
    FailureGenerator.cpp
    StarsNode.cpp
    LibStarsStatistics.cpp
    CentralizedScheduler.cpp
    PerformanceStatistics.cpp
    Properties.cpp
    SimAppDatabase.cpp
    SimTask.cpp
    SimulationCase.cpp
    Simulator.cpp
    TrafficStatistics.cpp
    SimOverlayBranch.cpp
    SimOverlayLeaf.cpp
    PolicyFactory.cpp
    SlaveLocalScheduler.cpp
    NetworkInterface.cpp
    ${cases_sources})

# add the executable
add_executable(stars-oldsim${BIN_SUFFIX} ${oldsim_sources})
target_link_libraries(stars-oldsim${BIN_SUFFIX}
    ${STARS_LIBRARIES}
    ${Boost_LIBRARIES}
    ${LOG4CPP_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    tcmalloc
)

#add_executable(stars-oldsim${BIN_SUFFIX}_static ${oldsim_sources})
#target_link_libraries(stars-oldsim${BIN_SUFFIX}_static -static -zmuldefs
#    ${STARS_LIBRARIES}_static
#    ${Boost_LIBRARIES}
#    ${LOG4CPP_LIBRARIES}
#    ${MSGPACK_LIBRARIES}
#    tcmalloc
#    unwind
#    z
#    pthread)
