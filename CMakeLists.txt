cmake_minimum_required(VERSION 3.20)
project(tatoemu VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(PLATFORM "native" CACHE STRING "Target platform")
message(STATUS "Building ${PLATFORM} version")

if(${PLATFORM} STREQUAL "native")
    add_subdirectory(platforms/native)
elseif(${PLATFORM} STREQUAL "wasm")
    add_subdirectory(platforms/wasm)
endif()
