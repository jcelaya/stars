include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIR})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(${MSGPACK_INCLUDE_DIR})
include_directories(../../include)

# library sources
add_subdirectory(Core)
add_subdirectory(Database)
add_subdirectory(ExecutionManager)
add_subdirectory(GlobalScheduler)
add_subdirectory(StructureManager)
add_subdirectory(SubmissionManager)
if (Wt_FOUND)
    include_directories(${Wt_INCLUDE_DIR})
    add_subdirectory(WtUI)
endif (Wt_FOUND)

# add the library
set(STARS_LIBRARIES stars${BIN_SUFFIX})
add_library(${STARS_LIBRARIES} SHARED ${stars_sources})

# add the executable
add_executable(stars-peer${BIN_SUFFIX} Core/main.cpp)
target_link_libraries(stars-peer${BIN_SUFFIX} ${STARS_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES} ${MSGPACK_LIBRARIES} ${Wt_LIBRARIES})
