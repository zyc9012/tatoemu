# Find SDL3 (Emscripten provides SDL3 via ports)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

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
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper009.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper010.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper019.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper023.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper024.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper025.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper026.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper069.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper073.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper074.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper162.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper163.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper164.cpp
    ${CMAKE_SOURCE_DIR}/emulators/nes/mappers/mapper178.cpp
    ${CMAKE_SOURCE_DIR}/emulators/emulator.cpp
    ${CMAKE_SOURCE_DIR}/platforms/wasm/main.cpp
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

# Include directories
target_include_directories(tatoemu PRIVATE 
    ${CMAKE_SOURCE_DIR}/emulators
)

# Link SDL3
target_link_libraries(tatoemu PRIVATE SDL3::SDL3)

set(SHELL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shell.html")

# Emscripten-specific settings
set(EMSCRIPTEN_LINK_FLAGS
    # Use SDL3 port
    "-sUSE_SDL=3"
    
    # Memory settings
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=128MB"
    "-sMAXIMUM_MEMORY=512MB"
    "-sSTACK_SIZE=5MB"
    
    # Export main function and C functions for file upload
    "-sEXPORTED_FUNCTIONS=['_main','_loadROMFile','_loadBootromFile']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
    
    # Environment
    "-sENVIRONMENT=web"
    
    # Enable filesystem for ROM loading
    "-sFORCE_FILESYSTEM=1"
    "-lidbfs.js"
    
    # Enable exceptions
    "-fexceptions"
    "-sNO_DISABLE_EXCEPTION_CATCHING"
    
    # Use custom HTML shell
    "--shell-file=${SHELL_FILE}"
)

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND EMSCRIPTEN_LINK_FLAGS
        "-O3"
        # Closure compiler disabled - causes issues with SDL3 exports
        # "--closure=1"
    )
    target_compile_options(tatoemu PRIVATE -O3)
else()
    list(APPEND EMSCRIPTEN_LINK_FLAGS
        "-O0"
        "-g"
        "-sASSERTIONS=1"
        "-sSTACK_OVERFLOW_CHECK=2"
    )
    target_compile_options(tatoemu PRIVATE -O0 -g)
endif()

# Apply linker flags
string(REPLACE ";" " " EMSCRIPTEN_LINK_FLAGS_STR "${EMSCRIPTEN_LINK_FLAGS}")
set_target_properties(tatoemu PROPERTIES
    LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS_STR}"
    OUTPUT_NAME "tatoemu"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    LINK_DEPENDS "${SHELL_FILE}"
)

# Compiler warnings
target_compile_options(tatoemu PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic
    -fexceptions
)

# Shell file for HTML
set_target_properties(tatoemu PROPERTIES
    SUFFIX ".html"
    OUTPUT_NAME "index"
)


