include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIR})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(../include)
include_directories(.)

# library sources
add_subdirectory(Core)
add_subdirectory(Database)
add_subdirectory(ExecutionManager)
add_subdirectory(GlobalScheduler)
add_subdirectory(StructureManager)
#add_subdirectory(SubmissionManager)

set(STARS_LIBRARIES stars-${CMAKE_BUILD_TYPE})

# add the executable
add_executable(stars-test TestSuite.cpp ${starstest_sources})
target_link_libraries(stars-test ${STARS_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES})

add_test(test-all stars-test)
