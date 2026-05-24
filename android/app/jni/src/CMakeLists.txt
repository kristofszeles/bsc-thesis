cmake_minimum_required(VERSION 3.22)

# android/app/jni/src -> repo root is four levels up
set(MAZE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")
set(MAZE_DIR "${MAZE_ROOT}/maze-game")

set(MAZE_SOURCES
    ${MAZE_DIR}/android_picker.cpp
    ${MAZE_DIR}/base64.cpp
    ${MAZE_DIR}/client.cpp
    ${MAZE_DIR}/drawutils.cpp
    ${MAZE_DIR}/editor.cpp
    ${MAZE_DIR}/entity.cpp
    ${MAZE_DIR}/game.cpp
    ${MAZE_DIR}/main.cpp
    ${MAZE_DIR}/map.cpp
    ${MAZE_DIR}/maze.cpp
    ${MAZE_DIR}/mesh.cpp
    ${MAZE_DIR}/window.cpp
)

add_library(main SHARED ${MAZE_SOURCES})

target_include_directories(main PRIVATE
    ${MAZE_DIR}
    ${MAZE_DIR}/include
)

target_compile_features(main PRIVATE cxx_std_20)

if(TARGET SDL2::SDL2main)
    target_link_libraries(main PRIVATE SDL2::SDL2main)
endif()
# GLES 3 / EGL: headers come from NDK; symbols live in libGLESv3.so / libEGL.so.
# zlib: gzip decompress (client.cpp) uses inflate* from libz.so.
find_library(GLESv3_LIB GLESv3)
find_library(EGL_LIB EGL)
find_library(Z_LIB z)
if(NOT GLESv3_LIB OR NOT EGL_LIB OR NOT Z_LIB)
    message(FATAL_ERROR "GLESv3/EGL/zlib not found (Android NDK sysroot issue)")
endif()

target_link_libraries(main PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_net::SDL2_net
    ${GLESv3_LIB}
    ${EGL_LIB}
    ${Z_LIB}
)
