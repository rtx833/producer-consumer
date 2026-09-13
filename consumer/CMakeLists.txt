add_executable(consumer
    consumer_app.cpp
    consumer_options.cpp
    consumer_reporter.cpp
    consumer_stats.cpp
    main.cpp
    packet_validator.cpp
    pause_policy.cpp
)
target_include_directories(consumer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(consumer PRIVATE pc_common pc_warnings)
