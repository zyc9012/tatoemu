cmake_minimum_required(VERSION 3.20)
project(gb-emu-wasm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Building WebAssembly version")

# Define source directory (parent of build directory)
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Source files (exclude SDL-specific files)
set(SOURCES
    ${SRC_DIR}/src/cpu.cpp
    ${SRC_DIR}/src/mmu.cpp
    ${SRC_DIR}/src/ppu.cpp
    ${SRC_DIR}/src/joypad.cpp
    ${SRC_DIR}/src/timer.cpp
    ${SRC_DIR}/src/cartridge.cpp
    ${SRC_DIR}/src/apu.cpp
    ${SRC_DIR}/src/bootrom.cpp
    ${SRC_DIR}/src/wasm/emulator_wasm.cpp
    ${SRC_DIR}/src/wasm/wasm_bindings.cpp
)

# Header files
set(HEADERS
    ${SRC_DIR}/include/cpu.h
    ${SRC_DIR}/include/mmu.h
    ${SRC_DIR}/include/ppu.h
    ${SRC_DIR}/include/joypad.h
    ${SRC_DIR}/include/timer.h
    ${SRC_DIR}/include/cartridge.h
    ${SRC_DIR}/include/apu.h
    ${SRC_DIR}/include/bootrom.h
    ${SRC_DIR}/include/types.h
    ${SRC_DIR}/include/wasm/emulator_wasm.h
)

# Create executable
add_executable(gb-emu ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(gb-emu PRIVATE 
    ${SRC_DIR}/include
)

# Emscripten-specific settings
set(EMSCRIPTEN_LINK_FLAGS
    # Enable embind for C++ bindings
    "--bind"
    
    # Memory settings
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=64MB"
    "-sMAXIMUM_MEMORY=512MB"
    
    # Export functions
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','getValue','setValue','HEAPU8','HEAPU32']"
    "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
    
    # Modularize output
    "-sMODULARIZE=1"
    "-sEXPORT_NAME='Module'"
    
    # Environment
    "-sENVIRONMENT=web"
    
    # Disable filesystem if not needed
    "-sNO_DISABLE_EXCEPTION_CATCHING"
    
    # Enable exceptions
    "-fexceptions"
)

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND EMSCRIPTEN_LINK_FLAGS
        "-O3"
        "--closure=1"
    )
    target_compile_options(gb-emu PRIVATE -O3)
else()
    list(APPEND EMSCRIPTEN_LINK_FLAGS
        "-O0"
        "-g"
        "-sASSERTIONS=1"
        "-sSTACK_OVERFLOW_CHECK=2"
    )
    target_compile_options(gb-emu PRIVATE -O0 -g)
endif()

# Apply linker flags
string(REPLACE ";" " " EMSCRIPTEN_LINK_FLAGS_STR "${EMSCRIPTEN_LINK_FLAGS}")
set_target_properties(gb-emu PROPERTIES
    LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS_STR}"
    OUTPUT_NAME "gb-emu"
)

# Compiler warnings
target_compile_options(gb-emu PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic
    -fexceptions
)

# Custom target to copy web directory and output files to binary directory
add_custom_command(TARGET gb-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SRC_DIR}/src/wasm/web
        ${CMAKE_BINARY_DIR}/web
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/gb-emu.js
        ${CMAKE_BINARY_DIR}/web/gb-emu.js
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/gb-emu.wasm
        ${CMAKE_BINARY_DIR}/web/gb-emu.wasm
)

