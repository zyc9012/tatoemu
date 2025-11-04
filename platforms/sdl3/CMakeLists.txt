# Find SDL3
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

# Source files
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/cpu.cpp
    ${CMAKE_SOURCE_DIR}/src/mmu.cpp
    ${CMAKE_SOURCE_DIR}/src/ppu.cpp
    ${CMAKE_SOURCE_DIR}/src/joypad.cpp
    ${CMAKE_SOURCE_DIR}/src/timer.cpp
    ${CMAKE_SOURCE_DIR}/src/cartridge.cpp
    ${CMAKE_SOURCE_DIR}/src/apu.cpp
    ${CMAKE_SOURCE_DIR}/src/bootrom.cpp
    ${CMAKE_SOURCE_DIR}/platforms/sdl3/emulator.cpp
    ${CMAKE_SOURCE_DIR}/platforms/sdl3/main.cpp
)

# Header files
set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/cpu.h
    ${CMAKE_SOURCE_DIR}/include/mmu.h
    ${CMAKE_SOURCE_DIR}/include/ppu.h
    ${CMAKE_SOURCE_DIR}/include/joypad.h
    ${CMAKE_SOURCE_DIR}/include/timer.h
    ${CMAKE_SOURCE_DIR}/include/cartridge.h
    ${CMAKE_SOURCE_DIR}/include/apu.h
    ${CMAKE_SOURCE_DIR}/include/bootrom.h
    ${CMAKE_SOURCE_DIR}/include/types.h
    ${CMAKE_SOURCE_DIR}/platforms/sdl3/emulator.h
)

# Create executable
add_executable(gb-emu ${SOURCES} ${HEADERS})

# Set output directory to build root
set_target_properties(gb-emu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Include directories
target_include_directories(gb-emu PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
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

