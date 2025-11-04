cmake_minimum_required(VERSION 3.20)
project(gb-emu VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(PLATFORM "sdl3" CACHE STRING "Target platform")
message(STATUS "Building ${PLATFORM} version")

if(${PLATFORM} STREQUAL "sdl3")
    add_subdirectory(platforms/sdl3)
elseif(${PLATFORM} STREQUAL "wasm")
    add_subdirectory(platforms/wasm)
endif()
