#include "engine/ImGuiLayer.h"
#include "engine/Profiler.h"
#include "engine/Stats.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <string>

namespace Genesis::Engine {

struct ImGuiLayer::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    entt::entity selectedEntity = entt::null;
};

ImGuiLayer::ImGuiLayer(SDL_Window* window, SDL_GLContext context)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->window = window;
    m_impl->context = context;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 330");
}

ImGuiLayer::~ImGuiLayer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::NewFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::Render(Profiler& /*profiler*/, Scene* scene) {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    ImGui::Begin("Genesis Engine");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Separator();
    ImGui::Text("Draw Calls: %d", Stats::GetDrawCalls());
    ImGui::End();

    if (scene) {
        ImGui::Begin("Scene Hierarchy");
        scene->Registry().each([&](auto entity) {
            std::string label = "Entity " + std::to_string((uint32_t)entity);
            if (ImGui::Selectable(label.c_str(), m_impl->selectedEntity == entity)) {
                m_impl->selectedEntity = entity;
            }
        });
        ImGui::End();

        ImGui::Begin("Inspector");
        if (m_impl->selectedEntity != entt::null && scene->Registry().valid(m_impl->selectedEntity)) {
            auto entity = m_impl->selectedEntity;
            
            if (scene->Registry().all_of<Transform>(entity)) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& tc = scene->Registry().get<Transform>(entity);
                    ImGui::DragFloat3("Position", &tc.x, 0.1f);
                    ImGui::DragFloat3("Rotation", &tc.rx, 0.1f);
                    ImGui::DragFloat3("Scale", &tc.sx, 0.1f);
                }
            }

            if (scene->Registry().all_of<LightComponent>(entity)) {
                if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& lc = scene->Registry().get<LightComponent>(entity);
                    const char* types[] = { "Directional", "Point" };
                    int currentType = (int)lc.type;
                    if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
                        lc.type = (LightType)currentType;
                    }
                    ImGui::ColorEdit3("Color", lc.color);
                    ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 100.0f);
                    if (lc.type == LightType::Point) {
                        ImGui::DragFloat("Range", &lc.range, 0.1f, 0.0f, 1000.0f);
                    }
                }
            }
            
            if (scene->Registry().all_of<ModelComponent>(entity)) {
                if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Model Loaded");
                }
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace Genesis::Engine
