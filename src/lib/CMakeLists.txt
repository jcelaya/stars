include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIR})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(${MSGPACK_INCLUDE_DIR})
include_directories(../../include)

# library sources
add_subdirectory(core)
add_subdirectory(db)
add_subdirectory(overlay)
add_subdirectory(scheduling)
add_subdirectory(ui)

# add the libraries
add_library(${STARS_LIBRARIES} SHARED ${stars_sources})
#add_library(${STARS_LIBRARIES}_static STATIC ${stars_sources})
add_library(${STARS_DB_LIBRARIES} SHARED ${stars_db_sources})

# add the executable
add_executable(stars-peer${BIN_SUFFIX} core/main.cpp)
target_link_libraries(stars-peer${BIN_SUFFIX}
    ${STARS_DB_LIBRARIES}
    ${STARS_UI_LIBRARIES}
    ${STARS_LIBRARIES}
    ${Boost_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    ${LOG4CPP_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    ${Wt_LIBRARIES})
