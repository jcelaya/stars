add_executable(cdf cdf.cpp ../Distributions.cpp)
target_link_libraries(cdf ${Boost_LIBRARIES})
