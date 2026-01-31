cmake_minimum_required(VERSION 3.20)
project(tatoemu VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(PLATFORM "native" CACHE STRING "Target platform")
message(STATUS "Building ${PLATFORM} version")

# Configure SDL build options
set(SDL_CAMERA OFF CACHE BOOL "Disable camera" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "Disable haptic" FORCE)
set(SDL_POWER OFF CACHE BOOL "Disable power" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "Disable sensor" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "Disable dialog" FORCE)
set(SDL_TRAY OFF CACHE BOOL "Disable tray" FORCE)
set(SDL_DISKAUDIO OFF CACHE BOOL "Disable disk audio" FORCE)
set(SDL_DUMMYAUDIO OFF CACHE BOOL "Disable dummy audio" FORCE)
set(SDL_DUMMYCAMERA OFF CACHE BOOL "Disable dummy camera" FORCE)
set(SDL_DUMMYVIDEO OFF CACHE BOOL "Disable dummy video" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "Disable HIDAPI" FORCE)
set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "Disable virtual joystick" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "Disable Vulkan" FORCE)
set(SDL_RENDER_VULKAN OFF CACHE BOOL "Disable Vulkan render" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "Disable DirectX" FORCE)
set(SDL_RENDER_D3D OFF CACHE BOOL "Disable Direct3D render" FORCE)
set(SDL_RENDER_D3D11 OFF CACHE BOOL "Disable Direct3D 11 render" FORCE)
set(SDL_RENDER_D3D12 OFF CACHE BOOL "Disable Direct3D 12 render" FORCE)
set(SDL_METAL OFF CACHE BOOL "Disable Metal" FORCE)
set(SDL_RENDER_METAL OFF CACHE BOOL "Disable Metal render" FORCE)
set(SDL_GPU OFF CACHE BOOL "Disable GPU" FORCE)
set(SDL_RENDER_GPU OFF CACHE BOOL "Disable GPU render" FORCE)
set(SDL_OFFSCREEN OFF CACHE BOOL "Disable offscreen" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "Disable OpenGLES" FORCE)
# set(SDL_OPENGL OFF CACHE BOOL "Disable OpenGL" FORCE)

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build static SDL library" FORCE)

# Build SDL from vendor directory
add_subdirectory(vendor/SDL)

if(${PLATFORM} STREQUAL "native")
    add_subdirectory(platforms/native)
elseif(${PLATFORM} STREQUAL "wasm")
    add_subdirectory(platforms/wasm)
endif()
