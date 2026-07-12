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
    "-sEXPORTED_FUNCTIONS=['_main','_loadROMFile','_loadBootromFile','_setScaleMode','_setVolume','_setNeoSys','_setNeoBios','_setKeyBinding','_cheatApply','_cheatAddCode','_cheatRemoveCode','_cheatToggleCode','_cheatClearCodes','_cheatGetCodesJson','_searchReset','_searchFilter','_searchCandidateCount','_searchGetCandidatesJson']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
    
    # Environment
    "-sENVIRONMENT=web"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createTatoEmuModule"
    
    # Enable filesystem for ROM loading
    "-sFORCE_FILESYSTEM=1"
    "-lidbfs.js"
    
    # Enable exceptions
    "-fexceptions"
    "-sNO_DISABLE_EXCEPTION_CATCHING"
    
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
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/emulator"
)

# Additional compiler options (warnings are in common file)
target_compile_options(tatoemu PRIVATE 
    -fexceptions
)

set_target_properties(tatoemu PROPERTIES
    SUFFIX ".js"
)

find_program(NPM_EXECUTABLE npm REQUIRED)

file(GLOB_RECURSE WEB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/web/src/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/package.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/package-lock.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/tsconfig.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/vite.config.ts"
)

add_custom_target(web_frontend ALL
    COMMAND ${CMAKE_COMMAND} -E env
            "TATOEMU_BUILD_DIR=${CMAKE_BINARY_DIR}"
            ${NPM_EXECUTABLE} run build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web"
    DEPENDS ${WEB_SOURCES}
    COMMENT "Building Preact frontend"
    VERBATIM
)

add_dependencies(web_frontend tatoemu)

