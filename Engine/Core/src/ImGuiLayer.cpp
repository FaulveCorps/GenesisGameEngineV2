#include "engine/ImGuiLayer.h"
#include "engine/Profiler.h"
#include "engine/Stats.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl2.h>
#include <SDL3/SDL.h>

#include <iostream>

namespace Genesis::Engine {

struct ImGuiLayer::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
};

ImGuiLayer::ImGuiLayer(SDL_Window* window, SDL_GLContext context)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->window = window;
    m_impl->context = context;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForOpenGL(window, context)) {
        std::cerr << "ImGui_ImplSDL3_InitForOpenGL failed" << std::endl;
    }
    if (!ImGui_ImplOpenGL2_Init()) {
        std::cerr << "ImGui_ImplOpenGL2_Init failed" << std::endl;
    }
}

ImGuiLayer::~ImGuiLayer() {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::NewFrame() {
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::Render(Profiler& profiler) {
    ImGui::Begin("Engine Stats");
    ImGui::Text("FPS: %.1f", profiler.GetFPS());
    ImGui::Text("Frame Time: %.2f ms", profiler.GetLastFrameMS());
    ImGui::Text("Draw Calls: %d", Genesis::Engine::Stats::GetDrawCalls());
    ImGui::Separator();
    ImGui::Text("Controls: Close window to exit");
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

} // namespace Genesis::Engine
