#pragma once

// MAZE_GLES marks platforms that render with OpenGL ES 3.0 (Android natively,
// Emscripten via WebGL 2). Desktop builds keep desktop OpenGL through GLEW.
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define MAZE_GLES 1
#endif

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#elif defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#endif
