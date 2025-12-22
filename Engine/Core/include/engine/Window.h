#pragma once

#include <string>
#include <SDL3/SDL.h>

namespace Genesis::Engine {

class Window {
public:
    Window();
    ~Window();

    bool Init(const std::string& title, int width = 1280, int height = 720);
    void Shutdown();

    // Returns true while the window should keep running
    bool PollEvents();

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
};

} // namespace Genesis::Engine
