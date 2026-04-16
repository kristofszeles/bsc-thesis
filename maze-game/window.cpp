#include <iostream>

#include "window.h"

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (type == GL_DEBUG_TYPE_ERROR) std::cout << "OpenGL error: " << message << std::endl;
}

Window::Window(const std::string& windowTitle, int windowWidth, int windowHeight, bool maximized, bool hidden) : windowTitle(windowTitle), windowWidth(windowWidth), windowHeight(windowHeight) {
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (maximized) flags |= SDL_WINDOW_MAXIMIZED;
    if (hidden) flags |= SDL_WINDOW_HIDDEN;
    exit = false;
    window = SDL_CreateWindow(windowTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);
    context = SDL_GL_CreateContext(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) perror("Glew: ");
    if (DEBUG_MODE) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(MessageCallback, 0);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    setViewportSize(windowWidth, windowHeight);
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::updateMenuFrame() {
    SDL_GL_SwapWindow(window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}