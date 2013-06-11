add_subdirectory(policies)

set(starstest_sources ${starstest_sources}
    scheduling/ExecutionMessagesTest.cpp
    scheduling/SchedulerTest.cpp
    scheduling/TestTask.cpp
    PARENT_SCOPE)
