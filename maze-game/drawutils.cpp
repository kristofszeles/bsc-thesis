#include <vector>
#include <fstream>
#include <GL/glew.h>

#include "drawutils.h"
#include "button.h"

void DrawUtils::drawTexture2D(Texture* texture, float x, float y, float scale, float width, float height) {
    if (!width) width = texture->getWidth() * scale;
    if (!height) height = texture->getHeight() * scale;
    GLfloat vertices[4][2] = { {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height} };
    GLfloat texCoords[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
    glBindTexture(GL_TEXTURE_2D, texture->getData());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, vertices);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, texCoords);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DrawUtils::drawRectangle(SDL_Color color, float x, float y, float width, float height) {
    GLfloat vertices[4][2] = { {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height} };
    GLfloat colors[4][3] = { {color.r/255.0f, color.g/255.0f, color.b/255.0f}, {color.r/255.0f, color.g/255.0f, color.b/255.0f}, {color.r/255.0f, color.g/255.0f, color.b/255.0f}, {color.r/255.0f, color.g/255.0f, color.b/255.0f} };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, vertices);
    glColorPointer(3, GL_FLOAT, sizeof(GLfloat) * 3, colors);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void DrawUtils::drawBackground2D(Texture* texture) {
    float textureWidth = 32;
    float textureHeight = 32;
    float windowWidth = (float)window->getWidth();
    float windowHeight = (float)window->getHeight();
    GLfloat vertices[4][2] = { {0, 0}, {windowWidth, 0}, {windowWidth, windowHeight}, {0, windowHeight} };
    GLfloat texCoords[4][2] = { {0, 0}, {windowWidth / textureWidth / 4, 0}, {windowWidth / textureWidth / 4, windowHeight / textureHeight / 4}, {0, windowHeight / textureHeight / 4} };
    glBindTexture(GL_TEXTURE_2D, texture->getData());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, vertices);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, texCoords);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DrawUtils::drawText(const std::string& text, float x, float y, float scale) {
    Texture texture(renderText(text, fonts));
    drawTexture2D(&texture, x, y, scale);
}

void DrawUtils::drawTextInput(const std::string& message, const std::string& input) {
    float scale = 4.0f;
    Texture texture1(renderText(message, fonts));
    Texture texture2(renderText(input + "_", fonts));
    drawTexture2D(&texture1, window->getWidth() / 2 - texture1.getWidth() * scale / 2, window->getHeight() / 2 - texture1.getHeight() * scale / 2 - 32, scale);
    drawTexture2D(&texture2, window->getWidth() / 2 - texture2.getWidth() * scale / 2, window->getHeight() / 2 - texture2.getHeight() * scale / 2 + 32, scale);
}

void DrawUtils::drawLabel(Texture* texture) {
    float scale = 6.0f;
    drawTexture2D(texture, window->getWidth() / 2 - texture->getWidth() * scale / 2, window->getHeight() / 2 - texture->getHeight() * scale, scale);
}

void DrawUtils::drawLogo(Texture* texture) {
    float scale = 8.0f;
    drawTexture2D(texture, window->getWidth() / 2 - texture->getWidth() * scale / 2, window->getHeight() / 2 - texture->getHeight() * scale - 100, scale);
}

void DrawUtils::drawAuthor(Texture* texture) {
    float scale = 2.0f;
    drawTexture2D(texture, 0, window->getHeight() - texture->getHeight() * scale, scale);
}

void DrawUtils::drawButtonGroup(const std::list<Button*>& buttons) {
    for (auto& button : buttons) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        float scale = button->getScale();
        float x = window->getWidth() / 2 - button->getWidth() * button->getScale() / 2;
        float y = button->getY() + window->getHeight() / 2 - button->getHeight() * scale;
        float w = button->getWidth() * scale * window->getViewportScaleX();
        float h = button->getHeight() * scale * window->getViewportScaleY();
        drawTexture2D(button->getTexture(), x, y, button->getScale());
        if (mouseX >= x && mouseX < x + w && mouseY >= y && mouseY < y + h) {
            drawTexture2D(textures->at("select"), x - 36, y, 2);
            drawTexture2D(textures->at("select"), x + button->getWidth() * button->getScale() + 8, y, 2);
        }
    }
}

SDL_Surface* loadImage(const std::string& fileName) {
    SDL_Surface* surface = IMG_Load(fileName.c_str());
    if (!surface) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "IMG_Load", IMG_GetError(), nullptr);
        exit(1);
    }
    return surface;
}

GLuint createTextureFromSurface(SDL_Surface* surface) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    return texture;
}

GLuint createTextureFromImage(const std::string& fileName) {
    SDL_Surface* surface = loadImage(fileName.c_str());
    GLuint texture = createTextureFromSurface(surface);
    SDL_FreeSurface(surface);
    return texture;
}

GLuint renderText(const std::string& text, SDL_Surface** fonts) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 854, 9, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Rect src = { 0, 0, 9, 9 };
    SDL_Rect dst = { 0, 0, 1000, 9 };
    for (unsigned int i = 0; i < text.size(); ++i) {
        if (text[i] >= '0' && text[i] <= '9') {
            int id = text[i] - '0';
            src.x = id * 9;
            SDL_BlitSurface(fonts[0], &src, surface, &dst);
            if (id == 1)
                dst.x += 4;
            else
                dst.x += 7;
        } else if (text[i] >= 'A' && text[i] <= 'Z') {
            int id = text[i] - 'A';
            src.x = id * 9;
            SDL_BlitSurface(fonts[1], &src, surface, &dst);
            if (id == 8)
                dst.x += 6;
            else if (id == 12)
                dst.x += 9;
            else if (id == 13 || id == 16 || id == 19 || id == 24)
                dst.x += 8;
            else if (id == 22)
                dst.x += 9;
            else
                dst.x += 7;
        } else if (text[i] >= 'a' && text[i] <= 'z') {
            int id = text[i] - 'a';
            src.x = id * 9;
            SDL_BlitSurface(fonts[2], &src, surface, &dst);
            if (id == 2 || id == 5 || id == 9 || id == 19)
                dst.x += 6;
            else if (id == 8 || id == 11)
                dst.x += 5;
            else if (id == 12 || id == 22)
                dst.x += 9;
            else
                dst.x += 7;
        } else if ((text[i] >= '!' && text[i] <= '/') || (text[i] >= ':' && text[i] <= '@') || (text[i] >= '[' && text[i] <= '`') || (text[i] >= '{' && text[i] <= '~')) {
            int id = 0;
            if (text[i] == '!')
                id = 0;
            else if (text[i] == '?')
                id = 1;
            else if (text[i] == '.')
                id = 2;
            else if (text[i] == ':')
                id = 3;
            else if (text[i] == '_')
                id = 4;
            else if (text[i] == '-')
                id = 5;
            else if (text[i] == '>')
                id = 6;
            src.x = id * 9;
            SDL_BlitSurface(fonts[3], &src, surface, &dst);
            if (id == 1)
                dst.x += 8;
            else if (id == 4)
                dst.x += 7;
            else if (id == 5)
                dst.x += 5;
            else if (id == 6)
                dst.x += 6;
            else
                dst.x += 4;
        } else {
            dst.x += 3;
        }
    }
    dst.w = dst.x;  // set width properly
    SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(0, dst.w, 9, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_BlitSurface(surface, nullptr, result, nullptr);
    GLuint texture = createTextureFromSurface(result);
    SDL_FreeSurface(surface);
    SDL_FreeSurface(result);
    return texture;
}