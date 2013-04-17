include_directories(${Boost_INCLUDE_DIRS})
include_directories(${LOG4CPP_INCLUDE_DIR})
include_directories(../../include)
include_directories(.)

set(LIBS ${STARS_LIBRARIES} ${STARS_DB_LIBRARIES} ${Boost_LIBRARIES} ${SQLITE3_LIBRARIES} ${LOG4CPP_LIBRARIES} ${MSGPACK_LIBRARIES} tcmalloc)

# add the executables
add_executable(twosp-aggrtest main.cpp TwoScalarParamsPT.cpp)
target_link_libraries(twosp-aggrtest ${LIBS})

add_executable(foursp-aggrtest main.cpp FourScalarParamsPT.cpp)
target_link_libraries(foursp-aggrtest ${LIBS})

add_executable(ibp-aggrtest main.cpp IBPAvailabilityInfoTest.cpp)
target_link_libraries(ibp-aggrtest ${LIBS})

add_executable(mmp-aggrtest main.cpp MMPAvailabilityInfoTest.cpp)
target_link_libraries(mmp-aggrtest ${LIBS})

add_executable(dp-aggrtest main.cpp DPAvailabilityInfoTest.cpp)
target_link_libraries(dp-aggrtest ${LIBS})

add_executable(msp-aggrtest main.cpp MSPAvailabilityInfoTest.cpp)
target_link_libraries(msp-aggrtest ${LIBS})

