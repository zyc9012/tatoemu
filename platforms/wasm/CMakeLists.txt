# Source files (exclude SDL-specific files)
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/cpu.cpp
    ${CMAKE_SOURCE_DIR}/src/mmu.cpp
    ${CMAKE_SOURCE_DIR}/src/ppu.cpp
    ${CMAKE_SOURCE_DIR}/src/joypad.cpp
    ${CMAKE_SOURCE_DIR}/src/timer.cpp
    ${CMAKE_SOURCE_DIR}/src/cartridge.cpp
    ${CMAKE_SOURCE_DIR}/src/apu.cpp
    ${CMAKE_SOURCE_DIR}/src/bootrom.cpp
    ${CMAKE_SOURCE_DIR}/platforms/wasm/emulator_wasm.cpp
    ${CMAKE_SOURCE_DIR}/platforms/wasm/wasm_bindings.cpp
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
    ${CMAKE_SOURCE_DIR}/platforms/wasm/emulator_wasm.h
)

# Create executable
add_executable(gb-emu ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(gb-emu PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
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
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
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
        ${CMAKE_CURRENT_SOURCE_DIR}/web
        ${CMAKE_BINARY_DIR}/web
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/gb-emu.js
        ${CMAKE_BINARY_DIR}/web/gb-emu.js
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/gb-emu.wasm
        ${CMAKE_BINARY_DIR}/web/gb-emu.wasm
)

