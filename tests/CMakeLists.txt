function(pc_add_unit_test name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/consumer
        ${CMAKE_SOURCE_DIR}/producer
    )
    target_link_libraries(${name} PRIVATE pc_common pc_warnings ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_library(pc_consumer_lib OBJECT ${CMAKE_SOURCE_DIR}/consumer/packet_validator.cpp)
target_include_directories(pc_consumer_lib PRIVATE ${CMAKE_SOURCE_DIR}/consumer)
target_link_libraries(pc_consumer_lib PRIVATE pc_common pc_warnings)

pc_add_unit_test(test_crc32)
pc_add_unit_test(test_ring_buffer)
pc_add_unit_test(test_cli)
pc_add_unit_test(test_packet_validator pc_consumer_lib)

add_test(NAME integration
    COMMAND ${CMAKE_COMMAND} -E env
        PRODUCER=$<TARGET_FILE:producer>
        CONSUMER=$<TARGET_FILE:consumer>
        bash ${CMAKE_CURRENT_SOURCE_DIR}/integration.sh
)
set_tests_properties(integration PROPERTIES TIMEOUT 180)
