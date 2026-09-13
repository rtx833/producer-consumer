cmake_minimum_required(VERSION 3.16)
project(producer_consumer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PC_BUILD_TESTS "Build unit and integration tests" ON)
option(PC_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(pc_warnings INTERFACE)
target_compile_options(pc_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual
)

if(PC_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Threads REQUIRED)

add_subdirectory(common)
add_subdirectory(producer)

if(PC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
