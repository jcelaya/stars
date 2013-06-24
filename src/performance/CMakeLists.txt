include_directories(${Boost_INCLUDE_DIRS})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(../../include)
include_directories(.)

set(LIBS ${STARS_LIBRARIES} ${STARS_DB_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES} ${MSGPACK_LIBRARIES} tcmalloc)
set(MAIN_SRCS main.cpp ../util/MemoryManager.cpp)

# add the executables
add_executable(ibp-aggrtest ${MAIN_SRCS} IBPAvailabilityInfoTest.cpp)
target_link_libraries(ibp-aggrtest ${LIBS})

add_executable(mmp-aggrtest ${MAIN_SRCS} MMPAvailabilityInfoTest.cpp)
target_link_libraries(mmp-aggrtest ${LIBS})

add_executable(dp-aggrtest ${MAIN_SRCS} DPAvailabilityInfoTest.cpp)
target_link_libraries(dp-aggrtest ${LIBS})

add_executable(msp-aggrtest ${MAIN_SRCS} MSPAvailabilityInfoTest.cpp)
target_link_libraries(msp-aggrtest ${LIBS})

