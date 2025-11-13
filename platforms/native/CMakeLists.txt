option(BUILD_STATIC "Build static executable." ON)

if(BUILD_STATIC AND NOT APPLE)
  find_package(SDL3
    REQUIRED COMPONENTS SDL3-static
    CONFIG)
else()
  find_package(SDL3
    REQUIRED COMPONENTS SDL3
    CONFIG)
endif()

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
    ${CMAKE_SOURCE_DIR}/src/emulator.cpp
    ${CMAKE_SOURCE_DIR}/platforms/native/main.cpp
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
    ${CMAKE_SOURCE_DIR}/include/emulator.h
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
if(BUILD_STATIC AND NOT APPLE)
  target_link_libraries(gb-emu PUBLIC SDL3::SDL3-static)
  set_target_properties(gb-emu PROPERTIES LINK_SEARCH_END_STATIC TRUE)
  target_link_options(gb-emu PRIVATE -static -s)
  if(WIN32)
    target_link_options(gb-emu PRIVATE -municode)
  endif()
else()
  target_link_libraries(gb-emu PUBLIC SDL3::SDL3)
endif()

# Compiler warnings and optimization flags
target_compile_options(gb-emu PRIVATE 
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    $<$<CONFIG:Debug>:-O0 -g>
)

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

# Install the executable
install(TARGETS gb-emu
    RUNTIME DESTINATION bin
)
