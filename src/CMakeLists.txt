include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIR})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(../include)

# library sources
add_subdirectory(Core)
add_subdirectory(Database)
add_subdirectory(ExecutionManager)
add_subdirectory(GlobalScheduler)
add_subdirectory(StructureManager)
add_subdirectory(SubmissionManager)
add_subdirectory(WtUI)

# add the library
set(STARS_LIBRARIES stars-${CMAKE_BUILD_TYPE})
add_library(${STARS_LIBRARIES} SHARED ${stars_sources})

# add the executable
add_executable(stars-peer Core/main.cpp)
target_link_libraries(stars-peer ${STARS_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES})
