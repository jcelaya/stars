include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIR})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(${MSGPACK_INCLUDE_DIR})
include_directories(../../include)
include_directories(.)

add_executable(sweeper sweeper.cpp SimulationsFile.cpp)

add_executable(sweeperdaemon sweeperdaemon.cpp SimulationsFile.cpp)
target_link_libraries(sweeperdaemon ${Boost_LIBRARIES})

add_executable(cdf cdf.cpp ../sim/Distributions.cpp)
target_link_libraries(cdf ${Boost_LIBRARIES})

set(STARS_LIBRARIES stars${BIN_SUFFIX})
set(STARS_DB_LIBRARIES stars_db${BIN_SUFFIX})

add_executable(aggrtest aggrtest.cpp)
target_link_libraries(aggrtest
    ${STARS_LIBRARIES}
    ${STARS_DB_LIBRARIES}
    ${Boost_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LOG4CPP_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    ${Wt_LIBRARIES})