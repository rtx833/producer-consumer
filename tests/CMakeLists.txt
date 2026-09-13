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

pc_add_unit_test(test_crc32)
pc_add_unit_test(test_ring_buffer)
pc_add_unit_test(test_cli)

