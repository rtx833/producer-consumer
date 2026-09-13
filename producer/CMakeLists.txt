add_executable(producer
    main.cpp
    packet_builder.cpp
    payload_generator.cpp
    producer_app.cpp
    producer_options.cpp
    producer_reporter.cpp
)
target_include_directories(producer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(producer PRIVATE pc_common pc_warnings)
