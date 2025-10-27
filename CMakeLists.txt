cmake_minimum_required(VERSION 3.20)
project(gb-emu VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Compiler warnings
if(MSVC)
    target_compile_options(gb-emu PRIVATE /W4)
else()
    target_compile_options(gb-emu PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
        -Wno-gnu-anonymous-struct
        -Wno-nested-anon-types
    )
endif()

