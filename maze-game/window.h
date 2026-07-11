#pragma once

#include "gl_core.h"
#include <SDL.h>
#include <SDL_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform2.hpp>
#include <string>

#include "camera.h"
#include "position.h"

class Window {
private:
    bool exit;
    std::string windowTitle;
    int windowWidth, windowHeight, viewportWidth, viewportHeight;
    SDL_Window* window;
    SDL_GLContext context;
#if !defined(MAZE_GLES)
    const bool DEBUG_MODE = false;
#endif
public:
    Window(const std::string& windowTitle, int windowWidth, int windowHeight, bool maximized = false, bool hidden = false);
    ~Window() {
        SDL_GL_DeleteContext(context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
    void updateMapFrame(glm::mat4& view, const Position& playerPos, Camera* camera);
    void updateMenuFrame();
    void toggleMaximized() {
        if (!isMaximized()) SDL_MaximizeWindow(window);
        else SDL_RestoreWindow(window);
    }
    void setWindowSize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
    }
    void setViewportSize(int width, int height) {
        viewportWidth = width;
        viewportHeight = height;
        glViewport(0, 0, viewportWidth, viewportHeight);
    }
    void setProjectionMatrixSize(int width, int height) {
#if !defined(MAZE_GLES)
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
#else
        (void)width;
        (void)height;
#endif
    }
    int getWidth() const { return windowWidth; }
    int getHeight() const { return windowHeight; }
    int getViewportWidth() const { return viewportWidth; }
    int getViewportHeight() const { return viewportHeight; }
    float getViewportScaleX() const { return (float)viewportWidth / windowWidth; }
    float getViewportScaleY() const { return (float)viewportHeight / windowHeight; }
    SDL_Window* getWindow() const { return window; }
    bool isMaximized() const {
        Uint32 flags = SDL_GetWindowFlags(window);
        if (flags & SDL_WINDOW_MAXIMIZED) return true;
        return false;
    }
};