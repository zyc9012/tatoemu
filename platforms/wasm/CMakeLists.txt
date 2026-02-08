# Find SDL3 (Emscripten provides SDL3 via ports)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

# Include common CMakeLists configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.common.txt)

# Source files (common + platform-specific)
set(SOURCES
    ${COMMON_SOURCES}
    ${CMAKE_SOURCE_DIR}/platforms/wasm/main.cpp
)

# Header files
set(HEADERS ${COMMON_HEADERS})

# Create executable
add_executable(tatoemu ${SOURCES} ${HEADERS})

# Apply common target settings
apply_common_target_settings(tatoemu)

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
    
    # Export main function and C functions for file upload and configuration
    "-sEXPORTED_FUNCTIONS=['_main','_loadROMFile','_loadBootromFile','_setScaleMode','_setVolume','_setNeoSys','_setNeoBios','_setKeyBinding']"
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

# Additional compiler options (warnings are in common file)
target_compile_options(tatoemu PRIVATE 
    -fexceptions
)

# Shell file for HTML
set_target_properties(tatoemu PROPERTIES
    SUFFIX ".html"
    OUTPUT_NAME "index"
)

# Define a custom target to copy assets
add_custom_target(
    copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/../../tatoemu.png
            ${CMAKE_BINARY_DIR}/dist/tatoemu.png
    COMMENT "Copying assets to the build directory"
)

add_dependencies(tatoemu copy_assets)

