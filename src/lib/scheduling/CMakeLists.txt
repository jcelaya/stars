add_subdirectory(policies)

set(stars_sources ${stars_sources}
    scheduling/DescriptionFile.cpp
    scheduling/Scheduler.cpp
    scheduling/Task.cpp
    scheduling/UnixExecutionEnvironment.cpp
    scheduling/SubmissionNode.cpp
    scheduling/SchedulingMsgExport.cpp
    PARENT_SCOPE)
