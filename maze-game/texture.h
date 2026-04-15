#pragma once

#include <string>

class Texture {
private:
    float width, height;
    GLuint texture;
public:
    Texture(GLuint texture) : texture(texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    ~Texture() { glDeleteTextures(1, &texture); }
    float getWidth() const { return width; }
    float getHeight() const { return height; }
    GLuint getData() const { return texture; }
};