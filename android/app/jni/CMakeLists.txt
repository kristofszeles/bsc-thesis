cmake_minimum_required(VERSION 3.22)
project(maze_android LANGUAGES C CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "This CMake tree is only for the Android NDK build (ANDROID must be set).")
endif()

set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

add_subdirectory(SDL)

set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL_image)

set(SDL2NET_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2NET_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL_net)

add_subdirectory(src)
