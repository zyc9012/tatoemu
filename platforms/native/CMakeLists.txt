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
    ${CMAKE_SOURCE_DIR}/emulators/gb/cpu.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/mmu.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/ppu.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/joypad.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/timer.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/cartridge.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/apu.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/bootrom.cpp
    ${CMAKE_SOURCE_DIR}/emulators/gb/core.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/cpu.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/memory.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/ppu.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/apu.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/cartridge.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/controller.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/core.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper000.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper001.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper002.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper003.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper004.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper005.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper010.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper019.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper023.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper024.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper025.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper026.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper073.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper074.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper163.cpp
    ${CMAKE_SOURCE_DIR}/emulators/emulator.cpp
    ${CMAKE_SOURCE_DIR}/platforms/native/main.cpp
)

# Header files
set(HEADERS
    ${CMAKE_SOURCE_DIR}/emulators/gb/cpu.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/mmu.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/ppu.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/joypad.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/timer.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/cartridge.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/apu.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/bootrom.h
    ${CMAKE_SOURCE_DIR}/emulators/gb/core.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/cpu.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/ppu.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/apu.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/memory.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/cartridge.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/controller.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/core.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/consts.h
    ${CMAKE_SOURCE_DIR}/emulators/nes/config.h
    ${CMAKE_SOURCE_DIR}/emulators/emulator.h
    ${CMAKE_SOURCE_DIR}/emulators/core.h
    ${CMAKE_SOURCE_DIR}/emulators/types.h
    ${CMAKE_SOURCE_DIR}/emulators/config.h
)

# Create executable
add_executable(tatoemu ${SOURCES} ${HEADERS})

# Set output directory to build root
set_target_properties(tatoemu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Include directories
target_include_directories(tatoemu PRIVATE 
    ${CMAKE_SOURCE_DIR}/emulators
)

# Link SDL3
if(BUILD_STATIC AND NOT APPLE)
  target_link_libraries(tatoemu PUBLIC SDL3::SDL3-static)
  set_target_properties(tatoemu PROPERTIES LINK_SEARCH_END_STATIC TRUE)
  target_link_options(tatoemu PRIVATE -static -s)
  if(WIN32)
    target_link_options(tatoemu PRIVATE -municode)
  endif()
else()
  target_link_libraries(tatoemu PUBLIC SDL3::SDL3)
endif()

# Compiler warnings and optimization flags
target_compile_options(tatoemu PRIVATE 
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
        set_property(TARGET tatoemu PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link Time Optimization (LTO) enabled")
    else()
        message(STATUS "LTO not supported: ${ipo_error}")
    endif()
endif()

# Install the executable
install(TARGETS tatoemu
    RUNTIME DESTINATION bin
)
