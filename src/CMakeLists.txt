# add the executable
include_directories(${Boost_INCLUDE_DIRS})
add_executable(stars main.cpp ConfigurationManager.cpp)
target_link_libraries(stars ${Boost_LIBRARIES})