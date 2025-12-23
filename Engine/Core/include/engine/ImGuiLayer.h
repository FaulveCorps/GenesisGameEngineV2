#pragma once

#include <SDL.h>
#include <memory>

namespace Genesis::Engine {

class Profiler;

class ImGuiLayer {
public:
    ImGuiLayer(SDL_Window* window, SDL_GLContext context);
    ~ImGuiLayer();

    void NewFrame();
    void Render(Profiler& profiler);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Genesis::Engine
