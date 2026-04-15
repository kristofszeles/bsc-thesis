#pragma once

#include <string>

#include "drawutils.h"

class Button {
private:
    float x, y, width, height, scale;
    Texture* texture;
public:
    Button(const std::string& text, float x, float y, float scale, SDL_Surface** fonts) : x(x), y(y), scale(scale) {
        texture = new Texture(renderText(text, fonts));
        width = texture->getWidth();
        height = texture->getHeight();
    }
    ~Button() { delete texture; }
    float getX() const { return x; }
    float getY() const { return y; }
    float getWidth() const { return width; }
    float getHeight() const { return height; }
    float getScale() const { return scale; }
    Texture* getTexture() const { return texture; }
};