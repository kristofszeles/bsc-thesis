#pragma once

// macOS legacy OpenGL 2.1 exposes VAOs via GL_APPLE_vertex_array_object; core glGenVertexArrays is NULL.
#include "gl_core.h"

inline void glCompatGenVertexArrays(GLsizei n, GLuint* ids) {
#ifdef __APPLE__
    if (GLEW_APPLE_vertex_array_object) {
        glGenVertexArraysAPPLE(n, ids);
        return;
    }
#endif
    glGenVertexArrays(n, ids);
}

inline void glCompatBindVertexArray(GLuint id) {
#ifdef __APPLE__
    if (GLEW_APPLE_vertex_array_object) {
        glBindVertexArrayAPPLE(id);
        return;
    }
#endif
    glBindVertexArray(id);
}

inline void glCompatDeleteVertexArrays(GLsizei n, GLuint* ids) {
#ifdef __APPLE__
    if (GLEW_APPLE_vertex_array_object) {
        glDeleteVertexArraysAPPLE(n, ids);
        return;
    }
#endif
    glDeleteVertexArrays(n, ids);
}

// GLES 3 allows program 0 in some specs, but several drivers mishandle it; leaving the last
// program bound is fine because the next draw binds an explicit program again.
inline void glCompatReleaseProgram() {
#if defined(__ANDROID__)
#else
    glUseProgram(0);
#endif
}
