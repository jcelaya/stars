include_directories(${Boost_INCLUDE_DIRS})

# add the executable
add_executable(sweeper sweeper.cpp SimulationsFile.cpp)
add_executable(sweeperdaemon daemon.cpp SimulationsFile.cpp)
target_link_libraries(sweeperdaemon ${Boost_LIBRARIES})
