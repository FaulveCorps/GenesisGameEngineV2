#pragma once

#include <SDL.h>
#include <memory>

namespace Genesis::Engine {

class Profiler;
class Scene;

class ImGuiLayer {
public:
    ImGuiLayer(SDL_Window* window, SDL_GLContext context);
    ~ImGuiLayer();

    void NewFrame();
    void Render(Profiler& profiler, Scene* scene);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Genesis::Engine
