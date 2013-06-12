set(starstest_sources ${starstest_sources}
    scheduling/policies/IntervalTest.cpp
    scheduling/policies/ScalarParameterTest.cpp
    scheduling/policies/DispatcherTest.cpp
    scheduling/policies/BasicAvailabilityInfoTest.cpp
    scheduling/policies/QueueBalancingInfoTest.cpp
    scheduling/policies/SlownessInformationTest.cpp
    scheduling/policies/TimeConstraintInfoTest.cpp
    scheduling/policies/TaskProxyTest.cpp
    scheduling/policies/RandomQueueGenerator.cpp
    PARENT_SCOPE)
