#include <vector>
#include <fstream>
#include <memory>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "drawutils.h"
#include "button.h"
#include "gl_compat.h"

namespace {
// specials.png: L/R use 6×7, U/D use 7×6 in each 9px column (treated as offset (1,1) in the cell).
void specialsTextureRectForId(int id, int& outW, int& outH, int& outSrcX, int& outSrcY) {
    if (id == 6 || id == 7) {
        outW = 6;
        outH = 8;
        outSrcX = id * 9;
        outSrcY = 0;
    } else if (id == 8 || id == 9) {
        outW = 7;
        outH = 7;
        outSrcX = id * 9;
        outSrcY = 0;
    } else {
        outW = 9;
        outH = 9;
        outSrcX = id * 9;
        outSrcY = 0;
    }
}
}  // namespace

// Menu chrome (main menu, text fields, option titles): 2x on Android only; desktop uses legacy sizes.
#if defined(__ANDROID__)
static constexpr float kMazeMenuUiFontScale = 2.0f;
#else
static constexpr float kMazeMenuUiFontScale = 1.0f;
#endif

#if defined(__ANDROID__)
#  include <SDL_log.h>
namespace {

glm::mat4 g_ortho2d(1.0f);
GLuint g_prog_tex = 0;
GLuint g_prog_color = 0;
GLint g_loc_tex_mvp = -1;
GLint g_loc_tex_sampler = -1;
GLint g_loc_col_mvp = -1;
GLint g_loc_col_color = -1;
GLuint g_ui_vao = 0;
GLuint g_ui_vbo = 0;

GLuint compileGlsl(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf((size_t)(len > 0 ? len : 1));
        glGetShaderInfoLog(s, len, nullptr, buf.data());
        SDL_Log("maze-game GLES shader compile failed: %s", buf.data());
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf((size_t)(len > 0 ? len : 1));
        glGetProgramInfoLog(p, len, nullptr, buf.data());
        SDL_Log("maze-game GLES program link failed: %s", buf.data());
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void ensureGles2D() {
    if (g_prog_tex != 0) return;

    static const char* vs_tex = R"(#version 300 es
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
uniform mat4 u_mvp;
out vec2 v_uv;
void main() {
  gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
  v_uv = a_uv;
})";

    static const char* fs_tex = R"(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 o_frag;
void main() {
  o_frag = texture(u_tex, v_uv);
})";

    static const char* vs_color = R"(#version 300 es
layout(location = 0) in vec2 a_pos;
uniform mat4 u_mvp;
uniform vec4 u_color;
out vec4 v_c;
void main() {
  gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
  v_c = u_color;
})";

    static const char* fs_color = R"(#version 300 es
precision mediump float;
in vec4 v_c;
out vec4 o_frag;
void main() {
  o_frag = v_c;
})";

    GLuint v1 = compileGlsl(GL_VERTEX_SHADER, vs_tex);
    GLuint f1 = compileGlsl(GL_FRAGMENT_SHADER, fs_tex);
    GLuint v2 = compileGlsl(GL_VERTEX_SHADER, vs_color);
    GLuint f2 = compileGlsl(GL_FRAGMENT_SHADER, fs_color);
    g_prog_tex = linkProgram(v1, f1);
    g_prog_color = linkProgram(v2, f2);
    g_loc_tex_mvp = glGetUniformLocation(g_prog_tex, "u_mvp");
    g_loc_tex_sampler = glGetUniformLocation(g_prog_tex, "u_tex");
    g_loc_col_mvp = glGetUniformLocation(g_prog_color, "u_mvp");
    g_loc_col_color = glGetUniformLocation(g_prog_color, "u_color");

    glGenVertexArrays(1, &g_ui_vao);
    glGenBuffers(1, &g_ui_vbo);
    glBindVertexArray(g_ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_ui_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, reinterpret_cast<void*>(sizeof(GLfloat) * 2));
    glBindVertexArray(0);
}

void drawTexturedQuad(GLuint tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float rotRad) {
    ensureGles2D();
    const float cx = x + 0.5f * w;
    const float cy = y + 0.5f * h;
    const float hx = 0.5f * w;
    const float hy = 0.5f * h;
    // Triangle strip: TL, TR, BL, BR
    const glm::vec2 loc[4] = { {-hx, -hy}, {hx, -hy}, {-hx, hy}, {hx, hy} };
    const glm::vec2 uv[4] = { {u0, v0}, {u1, v0}, {u0, v1}, {u1, v1} };
    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotRad, glm::vec3(0.0f, 0.0f, 1.0f));
    GLfloat data[16];
    for (int i = 0; i < 4; i++) {
        const glm::vec4 p = R * glm::vec4(loc[i], 0.0f, 1.0f);
        data[i * 4 + 0] = cx + p.x;
        data[i * 4 + 1] = cy + p.y;
        data[i * 4 + 2] = uv[i].x;
        data[i * 4 + 3] = uv[i].y;
    }
    glUseProgram(g_prog_tex);
    glUniformMatrix4fv(g_loc_tex_mvp, 1, GL_FALSE, glm::value_ptr(g_ortho2d));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(g_loc_tex_sampler, 0);
    glBindVertexArray(g_ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glCompatReleaseProgram();
}

void drawColorQuad(float x, float y, float w, float h, const GLfloat* rgba) {
    ensureGles2D();
    GLfloat data[8] = {
        x,     y,
        x + w, y,
        x,     y + h,
        x + w, y + h,
    };
    glUseProgram(g_prog_color);
    glUniformMatrix4fv(g_loc_col_mvp, 1, GL_FALSE, glm::value_ptr(g_ortho2d));
    glUniform4fv(g_loc_col_color, 1, rgba);
    glBindVertexArray(g_ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STREAM_DRAW);
    glDisableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, reinterpret_cast<void*>(sizeof(GLfloat) * 2));
    glBindVertexArray(0);
    glCompatReleaseProgram();
}

} // namespace
#endif

void DrawUtils::setGLES2DOrtho(float width, float height) {
#if defined(__ANDROID__)
    g_ortho2d = glm::ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
#else
    (void)width;
    (void)height;
#endif
}

void DrawUtils::drawTexture2D(Texture* texture, float x, float y, float scale, float width, float height, float rotationRadians) {
    if (!width) width = texture->getWidth() * scale;
    if (!height) height = texture->getHeight() * scale;
#if defined(__ANDROID__)
    drawTexturedQuad(texture->getData(), x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, rotationRadians);
#else
    const float cx = x + 0.5f * width;
    const float cy = y + 0.5f * height;
    const float hx = 0.5f * width;
    const float hy = 0.5f * height;
    // GL_QUADS: TL, TR, BR, BL
    const glm::vec2 loc[4] = { {-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy} };
    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotationRadians, glm::vec3(0.0f, 0.0f, 1.0f));
    GLfloat vertices[4][2];
    GLfloat texCoords[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
    for (int i = 0; i < 4; i++) {
        const glm::vec4 p = R * glm::vec4(loc[i], 0.0f, 1.0f);
        vertices[i][0] = cx + p.x;
        vertices[i][1] = cy + p.y;
    }
    glBindTexture(GL_TEXTURE_2D, texture->getData());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, vertices);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, texCoords);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

void DrawUtils::drawRectangle(SDL_Color color, float x, float y, float width, float height) {
#if defined(__ANDROID__)
    GLfloat rgba[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 1.0f };
    drawColorQuad(x, y, width, height, rgba);
#else
    GLfloat vertices[4][2] = { {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height} };
    GLfloat colors[4][3] = { {color.r/255.0f, color.g/255.0f, color.b/255.0f}, {color.r/255.0f, color.g/255.0f, color.b/255.0f}, {color.r/255.0f, color.g/255.0f, color.b/255.0f}, {color.r/255.0f, color.g/255.0f, color.b/255.0f} };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(GLfloat) * 2, vertices);
    glColorPointer(3, GL_FLOAT, sizeof(GLfloat) * 3, colors);
    glDrawArrays(GL_QUADS, 0, 4);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif
}

void DrawUtils::drawBackground2D(Texture* texture) {
    float textureWidth = 32;
    float textureHeight = 32;
    float windowWidth = (float)window->getWidth();
    float windowHeight = (float)window->getHeight();
#if defined(__ANDROID__)
    float u1 = windowWidth / textureWidth / 4;
    float v1 = windowHeight / textureHeight / 4;
    drawTexturedQuad(texture->getData(), 0, 0, windowWidth, windowHeight, 0, 0, u1, v1, 0.0f);
#else
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
#endif
}

void DrawUtils::drawText(const std::string& text, float x, float y, float scale) {
    std::unique_ptr<Texture> tex(renderText(text, fonts));
    drawTexture2D(tex.get(), x, y, scale);
}

void DrawUtils::drawTextInput(const std::string& message, const std::string& input) {
    const float scale = 8.0f;
    const float off = 64.0f;
    std::unique_ptr<Texture> texture1(renderText(message, fonts));
    std::unique_ptr<Texture> texture2(renderText(input + "_", fonts));
    drawTexture2D(texture1.get(), window->getWidth() / 2 - texture1->getWidth() * scale / 2, window->getHeight() / 2 - texture1->getHeight() * scale / 2 - off, scale);
    drawTexture2D(texture2.get(), window->getWidth() / 2 - texture2->getWidth() * scale / 2, window->getHeight() / 2 - texture2->getHeight() * scale / 2 + off, scale);
}

void DrawUtils::drawLabel(Texture* texture, float scale) {
	drawTexture2D(texture, window->getWidth() / 2 - texture->getWidth() * scale / 2, window->getHeight() / 2 - texture->getHeight() * scale, scale);
}

void DrawUtils::drawLogo(Texture* texture) {
    const float scale = 8.0f * kMazeMenuUiFontScale;
    const float up = 100.0f * kMazeMenuUiFontScale;
    drawTexture2D(texture, window->getWidth() / 2 - texture->getWidth() * scale / 2, window->getHeight() / 2 - texture->getHeight() * scale - up, scale);
}

void DrawUtils::drawAuthor(Texture* texture) {
    const float scale = 2.0f * kMazeMenuUiFontScale;
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
            const float selScale = 2.0f * kMazeMenuUiFontScale;
            const float selPad = 36.0f * kMazeMenuUiFontScale;
            const float selGap = 8.0f * kMazeMenuUiFontScale;
            drawTexture2D(textures->at("select"), x - selPad, y, selScale);
            drawTexture2D(textures->at("select"), x + button->getWidth() * button->getScale() + selGap, y, selScale);
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

Texture* createTextureFromSurface(SDL_Surface* surface) {
    GLuint texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
#if !defined(__ANDROID__)
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
#else
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
#if defined(__ANDROID__)
    glGenerateMipmap(GL_TEXTURE_2D);
#endif
    glBindTexture(GL_TEXTURE_2D, 0);
    return new Texture(texId, static_cast<float>(surface->w), static_cast<float>(surface->h));
}

Texture* createTextureFromImage(const std::string& fileName) {
    SDL_Surface* surface = loadImage(fileName.c_str());
    Texture* t = createTextureFromSurface(surface);
    SDL_FreeSurface(surface);
    return t;
}

Texture* renderSpecialsGlyph(int id, SDL_Surface** fonts) {
    if (id < 0) {
        id = 0;
    }
    if (id > 9) {
        id = 9;
    }
    int w, h, sx, sy;
    specialsTextureRectForId(id, w, h, sx, sy);
    SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Rect src = {sx, sy, w, h};
    SDL_Rect dst = {0, 0, w, h};
    SDL_BlitSurface(fonts[3], &src, result, &dst);
    Texture* texture = createTextureFromSurface(result);
#if defined(__ANDROID__)
    // Glyphs are tiny; default surface upload uses REPEAT + mipmaps which can make edge samples wrap
    // or over-minify, clipping the left/top texels. Chevrons need clamp + base-level nearest.
    {
        const GLuint tid = texture->getData();
        glBindTexture(GL_TEXTURE_2D, tid);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
#endif
    SDL_FreeSurface(result);
    return texture;
}

Texture* renderText(const std::string& text, SDL_Surface** fonts) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 854, 9, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Rect src = { 0, 0, 9, 9 };
    SDL_Rect dst = { 0, 0, 1000, 9 };
    int lineRightMax = 0;  // for U/D 7px wide with 6px advance, extend result width
    for (unsigned int i = 0; i < text.size(); ++i) {
        if (text[i] >= '0' && text[i] <= '9') {
            int id = text[i] - '0';
            src.x = id * 9;
            const int penX = dst.x;
            SDL_BlitSurface(fonts[0], &src, surface, &dst);
            lineRightMax = (std::max)(lineRightMax, penX + 9);
            if (id == 1)
                dst.x += 4;
            else
                dst.x += 7;
        } else if (text[i] >= 'A' && text[i] <= 'Z') {
            int id = text[i] - 'A';
            src.x = id * 9;
            const int penX = dst.x;
            SDL_BlitSurface(fonts[1], &src, surface, &dst);
            lineRightMax = (std::max)(lineRightMax, penX + 9);
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
            const int penX = dst.x;
            SDL_BlitSurface(fonts[2], &src, surface, &dst);
            lineRightMax = (std::max)(lineRightMax, penX + 9);
            if (id == 2 || id == 5 || id == 9 || id == 19)
                dst.x += 6;
            else if (id == 8 || id == 11)
                dst.x += 5;
            else if (id == 12 || id == 22)
                dst.x += 9;
            else
                dst.x += 7;
        } else if ((text[i] >= '!' && text[i] <= '/') || (text[i] >= ':' && text[i] <= '@') || (text[i] >= '[' && text[i] <= '`') || (text[i] >= '{' && text[i] <= '~')) {
            // fonts[3] specials.png: ! ? . : - = > < ^ v  (ids 0..9). Down chevron is id 9 — use renderSpecialsGlyph(9). '_' uses slot 4 (hyphen art).
            int id = 0;
            if (text[i] == '!') {
                id = 0;
            } else if (text[i] == '?') {
                id = 1;
            } else if (text[i] == '.') {
                id = 2;
            } else if (text[i] == ':') {
                id = 3;
            } else if (text[i] == '-' || text[i] == '_') {
                id = 4;
            } else if (text[i] == '=') {
                id = 5;
            } else if (text[i] == '>') {
                id = 6;
            } else if (text[i] == '<') {
                id = 7;
            } else if (text[i] == '^') {
                id = 8;
            } else {
                id = 0;
            }
            const int penX = dst.x;
            if (id == 6 || id == 7) {
                // Match specialsTextureRectForId(): chevron artwork is 6x8 at the top-left of
                // the 9px cell, not the 6x7-at-(1,1) the previous rect was reading (which
                // clipped the top row and left column — the visible "truncation" on `>`).
                src = {id * 9, 0, 6, 8};
                dst = {penX, 0, 6, 8};
                SDL_BlitSurface(fonts[3], &src, surface, &dst);
                lineRightMax = (std::max)(lineRightMax, penX + 6);
                // Restore the standard 9x9 blit rect — digit/letter branches only update src.x
                // and dst.x, so leaving src/dst at 6x8 here truncates every following glyph.
                src = {0, 0, 9, 9};
                dst = {penX + 6, 0, 1000, 9};
            } else if (id == 8) {
                src = {id * 9, 0, 7, 7};
                dst = {penX, 0, 7, 7};
                SDL_BlitSurface(fonts[3], &src, surface, &dst);
                lineRightMax = (std::max)(lineRightMax, penX + 7);
                src = {0, 0, 9, 9};
                dst = {penX + 6, 0, 1000, 9};
            } else {
                src = {id * 9, 0, 9, 9};
                dst = {penX, 0, 9, 9};
                SDL_BlitSurface(fonts[3], &src, surface, &dst);
                lineRightMax = (std::max)(lineRightMax, penX + 9);
                if (id == 1) {
                    dst.x = penX + 8;
                } else if (id == 4) {
                    dst.x = penX + 5;
                } else if (id == 5) {
                    dst.x = penX + 6;
                } else {
                    dst.x = penX + 4;
                }
            }
        } else {
            dst.x += 3;
        }
    }
    if (lineRightMax > 0) {
        dst.w = (std::max)(lineRightMax, dst.x);
    } else {
        dst.w = dst.x;
    }
    SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(0, dst.w, 9, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_BlitSurface(surface, nullptr, result, nullptr);
    Texture* texture = createTextureFromSurface(result);
    SDL_FreeSurface(surface);
    SDL_FreeSurface(result);
    return texture;
}
