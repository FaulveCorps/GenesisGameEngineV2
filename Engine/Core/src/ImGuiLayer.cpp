#include "engine/ImGuiLayer.h"
#include "engine/Profiler.h"
#include "engine/Stats.h"
#include <SDL.h>

namespace Genesis::Engine {

struct ImGuiLayer::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
};

ImGuiLayer::ImGuiLayer(SDL_Window* window, SDL_GLContext context)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->window = window;
    m_impl->context = context;
}

ImGuiLayer::~ImGuiLayer() = default;

void ImGuiLayer::NewFrame() {}

void ImGuiLayer::Render(Profiler& /*profiler*/) {}

} // namespace Genesis::Engine
