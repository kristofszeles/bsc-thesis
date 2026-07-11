#include <iostream>

#include "window.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#include "web_support.h"

// Keep the SDL window (and therefore the canvas render size) in sync with the
// browser window. SDL_SetWindowSize resizes the canvas and emits
// SDL_WINDOWEVENT_SIZE_CHANGED, which the existing event handlers pick up.
static EM_BOOL mazeWebOnResize(int eventType, const EmscriptenUiEvent* uiEvent, void* userData) {
    (void)eventType;
    SDL_Window* window = static_cast<SDL_Window*>(userData);
    if (uiEvent->windowInnerWidth > 0 && uiEvent->windowInnerHeight > 0) {
        SDL_SetWindowSize(window, uiEvent->windowInnerWidth, uiEvent->windowInnerHeight);
    }
    return EM_TRUE;
}
#endif

#if !defined(MAZE_GLES)
void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (type == GL_DEBUG_TYPE_ERROR) std::cout << "OpenGL error: " << message << std::endl;
}
#endif

Window::Window(const std::string& windowTitle, int windowWidth, int windowHeight, bool maximized, bool hidden) : windowTitle(windowTitle), windowWidth(windowWidth), windowHeight(windowHeight) {
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (maximized) flags |= SDL_WINDOW_MAXIMIZED;
    if (hidden) flags |= SDL_WINDOW_HIDDEN;
    exit = false;
#if defined(MAZE_GLES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
#if defined(__EMSCRIPTEN__)
    // The canvas fills the browser window (see shell.html); the config's saved
    // window dimensions are meaningless here, so size to the browser instead.
    {
        const int browserW = maze_web::browserWindowWidth();
        const int browserH = maze_web::browserWindowHeight();
        if (browserW > 0 && browserH > 0) {
            windowWidth = browserW;
            windowHeight = browserH;
        }
    }
#endif
    window = SDL_CreateWindow(windowTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);
    context = SDL_GL_CreateContext(window);
#if defined(__EMSCRIPTEN__)
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, window, EM_FALSE, mazeWebOnResize);
#endif
#if !defined(MAZE_GLES)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) perror("Glew: ");
    if (DEBUG_MODE) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(MessageCallback, 0);
    }
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // The OS may resize the window away from the requested dimensions (Android forces fullscreen
    // to the device surface; maximized desktop windows snap to the workspace). Pick up the real
    // size now so the loading screen, drawn before any SIZE_CHANGED event, uses correct extents.
    // Assign to the members explicitly — the constructor's parameters shadow them, so plain
    // `windowWidth = ...` would only update the locals and leave getWidth()/getHeight() stale.
    int actualW = this->windowWidth, actualH = this->windowHeight;
    SDL_GetWindowSize(window, &actualW, &actualH);
    if (actualW > 0 && actualH > 0) {
        this->windowWidth = actualW;
        this->windowHeight = actualH;
    }
    setViewportSize(this->windowWidth, this->windowHeight);
    setProjectionMatrixSize(this->windowWidth, this->windowHeight);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SDL_GL_SetSwapInterval(1); // enable vsync
}

void Window::updateMapFrame(glm::mat4& view, const Position& playerPos, Camera* camera) {
    glm::vec3 up(0, 1, 0);
    if (camera->getMode() == 0) {
        glm::vec3 position(playerPos.x, playerPos.y + 1, playerPos.z);
        view = glm::lookAt(position, position - camera->getPosition(), up);
    } else if (camera->getMode() == 1) {
        glm::vec3 position(playerPos.x, 0, playerPos.z);
        glm::vec3 direction(playerPos.x, 0, playerPos.z);
        view = glm::lookAt(position + camera->getPosition(), direction, up);
    } else if (camera->getMode() == 2) {
        glm::vec3 position(0, 0, 2);
        glm::vec3 direction(0, 0, -2);
        view = glm::lookAt(position, direction, up);
    }
    SDL_GL_SwapWindow(window);
#if defined(__EMSCRIPTEN__)
    maze_web::frameYield();
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::updateMenuFrame() {
    SDL_GL_SwapWindow(window);
#if defined(__EMSCRIPTEN__)
    maze_web::frameYield();
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
