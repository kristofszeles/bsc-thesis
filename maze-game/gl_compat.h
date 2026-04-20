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
