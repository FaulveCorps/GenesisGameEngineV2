#include "engine/Window.h"
#include <iostream>

namespace Genesis::Engine {

Window::Window() = default;
Window::~Window() { Shutdown(); }

bool Window::Init(const std::string& title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // SDL3 has a simplified SDL_CreateWindow signature (no position args)
    m_window = SDL_CreateWindow(title.c_str(),
                                width,
                                height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    // Create a GL context (used by future renderer)
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
        return false;
    }

    return true;
}

void Window::Shutdown() {
    if (m_glContext) {
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

bool Window::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            return false;
    }
    return true;
}

} // namespace Genesis::Engine
