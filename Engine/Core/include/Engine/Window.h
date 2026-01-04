#pragma once

#include <string>
#include <SDL.h>

namespace Genesis::Engine {

class Window {
public:
    Window();
    ~Window();

    bool Init(const std::string& title, int width = 1280, int height = 720);
    void Shutdown();

    // Returns true while the window should keep running
    // Optional callback for processing SDL events (e.g. for ImGui)
    using EventCallback = void(*)(const SDL_Event&);
    bool PollEvents(EventCallback callback = nullptr);

    // Access to underlying SDL window / context for renderers
    SDL_Window* GetSDLWindow() const { return m_window; }
    SDL_GLContext GetGLContext() const { return m_glContext; }
    uint32_t GetWindowID() const { return m_windowId; }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    uint32_t m_windowId = 0;
};

} // namespace Genesis::Engine
