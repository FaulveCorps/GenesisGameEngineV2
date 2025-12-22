#include "engine/OpenGLRenderer.h"
#include <SDL3/SDL.h>
#include <iostream>

namespace Genesis::Engine {

bool OpenGLRenderer::Init(SDL_Window* window, SDL_GLContext glContext) {
    if (!window || !glContext) {
        std::cerr << "OpenGLRenderer: invalid window or GL context" << std::endl;
        return false;
    }

    m_window = window;
    m_context = glContext;

    // Make sure the GL context is current on this thread
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Basic GL init
    glViewport(0, 0, 1280, 720);
    glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    return true;
}

void OpenGLRenderer::BeginFrame() {
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed in BeginFrame: " << SDL_GetError() << std::endl;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::EndFrame() {
    SDL_GL_SwapWindow(m_window);
}

void OpenGLRenderer::Shutdown() {
    // Nothing platform-specific here; Window owns the GL context
    m_window = nullptr;
    m_context = nullptr;
}

} // namespace Genesis::Engine
