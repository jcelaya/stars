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

# add the libraries
set(STARS_LIBRARIES stars${BIN_SUFFIX})
add_library(${STARS_LIBRARIES} SHARED ${stars_sources})
set(STARS_DB_LIBRARIES stars_db${BIN_SUFFIX})
add_library(${STARS_DB_LIBRARIES} SHARED ${stars_db_sources})

# Web UI library
if (Wt_FOUND)
    include_directories(${Wt_INCLUDE_DIR})
    add_subdirectory(WtUI)
	set(STARS_UI_LIBRARIES stars_ui${BIN_SUFFIX})
	add_library(${STARS_UI_LIBRARIES} SHARED ${stars_ui_sources})
endif (Wt_FOUND)

# add the executable
add_executable(stars-peer${BIN_SUFFIX} Core/main.cpp)
target_link_libraries(stars-peer${BIN_SUFFIX}
    ${STARS_DB_LIBRARIES}
    ${STARS_UI_LIBRARIES}
    ${STARS_LIBRARIES}
    ${Boost_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LOG4CPP_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    ${Wt_LIBRARIES})
