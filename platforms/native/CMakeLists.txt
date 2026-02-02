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

# Include common CMakeLists configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.common.txt)

# Source files (common + platform-specific)
set(SOURCES
    ${COMMON_SOURCES}
    ${CMAKE_SOURCE_DIR}/platforms/native/main.cpp
    ${CMAKE_SOURCE_DIR}/platforms/native/configfile.cpp
)

# Header files
set(HEADERS ${COMMON_HEADERS})

# Create executable
add_executable(tatoemu ${SOURCES} ${HEADERS})

# Apply common target settings
apply_common_target_settings(tatoemu)

# Set output directory to build root
set_target_properties(tatoemu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Embed Windows icon into the output binary
if(WIN32)
  set(TATOEMU_ICO_PATH "${CMAKE_SOURCE_DIR}/platforms/native/tatoemu.ico")
  if(EXISTS "${TATOEMU_ICO_PATH}")
    configure_file(
      "${CMAKE_CURRENT_LIST_DIR}/tatoemu.rc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/tatoemu.rc"
      @ONLY
    )
    target_sources(tatoemu PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/tatoemu.rc")
  endif()
endif()

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

# Compiler optimization flags
target_compile_options(tatoemu PRIVATE 
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
