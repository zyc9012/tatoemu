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

# Find SDL3
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

# Source files
set(SOURCES
    src/main.cpp
    src/cpu.cpp
    src/mmu.cpp
    src/ppu.cpp
    src/joypad.cpp
    src/timer.cpp
    src/emulator.cpp
    src/cartridge.cpp
    src/apu.cpp
)

# Header files
set(HEADERS
    include/cpu.h
    include/mmu.h
    include/ppu.h
    include/joypad.h
    include/timer.h
    include/emulator.h
    include/cartridge.h
    include/apu.h
    include/types.h
)

# Create executable
add_executable(gb-emu ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(gb-emu PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link SDL3
target_link_libraries(gb-emu PRIVATE SDL3::SDL3)

# Compiler warnings and optimization flags
if(MSVC)
    target_compile_options(gb-emu PRIVATE 
        /W4
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi>
    )
else()
    target_compile_options(gb-emu PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
        $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
        $<$<CONFIG:Debug>:-O0 -g>
    )
endif()

# Enable Link Time Optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set_property(TARGET gb-emu PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link Time Optimization (LTO) enabled")
    else()
        message(STATUS "LTO not supported: ${ipo_error}")
    endif()
endif()

