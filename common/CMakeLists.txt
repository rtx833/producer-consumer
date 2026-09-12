add_library(pc_common STATIC
    src/byte_size.cpp
    src/checksum.cpp
    src/cli.cpp
)
target_include_directories(pc_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pc_common
    PUBLIC Threads::Threads
    PRIVATE pc_warnings
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # shm_open/shm_unlink live in librt on older glibc versions.
    target_link_libraries(pc_common PUBLIC rt)
endif()
