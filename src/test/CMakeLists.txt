include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIR})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(${MSGPACK_INCLUDE_DIR})
include_directories(../../include)
include_directories(.)

# library sources
add_subdirectory(Core)
add_subdirectory(Database)
add_subdirectory(scheduling)
add_subdirectory(StructureManager)
#add_subdirectory(SubmissionManager)

# add the executable
add_executable(stars-test${BIN_SUFFIX} TestSuite.cpp ${starstest_sources} ../util/SignalException.cpp)
target_link_libraries(stars-test${BIN_SUFFIX}
    ${STARS_LIBRARIES}
    ${STARS_DB_LIBRARIES}
    ${Boost_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LOG4CPP_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    ${Wt_LIBRARIES})

add_test(test-all ${CMAKE_CURRENT_BINARY_DIR}/stars-test${BIN_SUFFIX} --log_level=test_suite --run_test=Cor)
