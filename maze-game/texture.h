#pragma once

#include "gl_core.h"

class Texture {
private:
    float width{}, height{};
    GLuint texture{};
public:
    Texture(GLuint tex, float w, float h) : width(w), height(h), texture(tex) {}

    Texture(Texture&& o) noexcept : width(o.width), height(o.height), texture(o.texture) { o.texture = 0; }

    Texture& operator=(Texture&& o) noexcept {
        if (this != &o) {
            if (texture != 0) glDeleteTextures(1, &texture);
            width = o.width;
            height = o.height;
            texture = o.texture;
            o.texture = 0;
        }
        return *this;
    }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    ~Texture() {
        if (texture != 0) glDeleteTextures(1, &texture);
    }

    /** Transfer GL name to caller; destructor will not delete it. */
    GLuint release() noexcept {
        GLuint t = texture;
        texture = 0;
        return t;
    }

    float getWidth() const { return width; }
    float getHeight() const { return height; }
    GLuint getData() const { return texture; }
};
