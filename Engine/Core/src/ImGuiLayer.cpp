#include "engine/ImGuiLayer.h"
#include "engine/Profiler.h"
#include "engine/Stats.h"
#include "engine/Engine.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/IAudio.h"
#include "engine/EditorHelpers.h"
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <string>
#include <iostream>
#include <typeinfo>

namespace Genesis::Engine {

// Helper to draw components with a consistent style
template<typename T, typename UIFunction>
static void DrawComponent(const std::string& name, entt::registry& registry, entt::entity entity, UIFunction uiFunction) {
    if (registry.all_of<T>(entity)) {
        auto& component = registry.get<T>(entity);
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;
        
        bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
        ImGui::PopStyleVar();

        bool removeComponent = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Remove Component"))
                removeComponent = true;
            ImGui::EndPopup();
        }

        if (open) {
            uiFunction(component);
            ImGui::TreePop();
        }

        if (removeComponent)
            registry.remove<T>(entity);
    }
}

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
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows

    // Load Monoid font
    ImFont* font = io.Fonts->AddFontFromFileTTF("Assets/fonts/Monoid.ttf", 18.0f);
    if (font) {
        // Build atlas now to ensure it works
        // io.Fonts->Build(); // Normally handled by backend
        std::cout << "Successfully loaded Monoid font." << std::endl;
    } else {
        std::cerr << "Failed to load Monoid font from Assets/fonts/Monoid.ttf" << std::endl;
        // Fallback to default
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

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

            auto& registry = scene->Registry();

            DrawComponent<CameraComponent>("Camera", registry, entity, [](auto& component) {
                ImGui::Checkbox("Primary", &component.primary);
                ImGui::DragFloat("FOV", &component.fov, 0.1f);
                ImGui::DragFloat("Near Plane", &component.nearPlane, 0.1f);
                ImGui::DragFloat("Far Plane", &component.farPlane, 0.1f);
            });

            DrawComponent<RigidBodyComponent>("Rigid Body", registry, entity, [](auto& component) {
                ImGui::DragFloat("Mass", &component.mass, 0.1f);
                ImGui::Checkbox("Use Gravity", &component.useGravity);
                ImGui::Checkbox("Is Kinematic", &component.isKinematic);
            });

            DrawComponent<BoxColliderComponent>("Box Collider", registry, entity, [](auto& component) {
                ImGui::DragFloat3("Size", component.size, 0.1f);
                ImGui::DragFloat3("Offset", component.offset, 0.1f);
                ImGui::Checkbox("Is Trigger", &component.isTrigger);
            });

            DrawComponent<SphereColliderComponent>("Sphere Collider", registry, entity, [](auto& component) {
                ImGui::DragFloat("Radius", &component.radius, 0.1f);
                ImGui::DragFloat3("Offset", component.offset, 0.1f);
                ImGui::Checkbox("Is Trigger", &component.isTrigger);
            });

            // Audio Component
            DrawComponent<AudioComponent>("Audio", registry, entity, [](auto& component) {
                ImGui::TextWrapped("Source: %s", component.soundPath.empty() ? "(unspecified)" : component.soundPath.c_str());
                if (ImGui::Button("Play")) {
                    PlayAudioPreview(component.soundPath, component.volume);
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    auto s = GetAudioSubsystem();
                    if (s) s->StopAll();
                }
                char buf[256];
                if (component.soundPath.length() >= 256) buf[0] = 0; else strcpy_s(buf, component.soundPath.c_str());
                if (ImGui::InputText("Sound Path", buf, 256)) {
                    component.soundPath = buf;
                }
                ImGui::DragFloat("Volume", &component.volume, 0.01f, 0.0f, 4.0f);
                ImGui::DragFloat("Pitch", &component.pitch, 0.01f, 0.1f, 4.0f);
                ImGui::Checkbox("Loop", &component.loop);
                ImGui::Checkbox("Spatial", &component.spatial);
                ImGui::DragFloat("Min Distance", &component.minDistance, 0.1f);
                ImGui::DragFloat("Max Distance", &component.maxDistance, 0.1f);
                ImGui::Checkbox("Play On Awake", &component.playOnAwake);
            });

            // Particle System
            DrawComponent<ParticleSystemComponent>("Particle System", registry, entity, [&](auto& component) {
                ImGui::DragFloat("Duration", &component.duration, 0.1f);
                ImGui::Checkbox("Looping", &component.looping);
                ImGui::Checkbox("Play On Awake", &component.playOnAwake);
                ImGui::DragFloat("Start Lifetime", &component.startLifetime, 0.1f);
                ImGui::DragFloat("Start Speed", &component.startSpeed, 0.1f);
                ImGui::DragFloat("Start Size", &component.startSize, 0.01f);
                ImGui::ColorEdit4("Start Color", component.startColor);
                ImGui::DragFloat("Rate Over Time", &component.rateOverTime, 0.1f);
                ImGui::DragFloat("Emitter Radius", &component.emitterRadius, 0.01f);
                if (ImGui::Button("Play")) {
                    StartParticlePreview(registry, entity);
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    StopParticlePreview(registry, entity);
                }
            });

            ImGui::Separator();

            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("AddComponent");

            if (ImGui::BeginPopup("AddComponent")) {
                if (!registry.all_of<CameraComponent>(entity) && ImGui::MenuItem("Camera")) {
                    registry.emplace<CameraComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
                if (!registry.all_of<RigidBodyComponent>(entity) && ImGui::MenuItem("Rigid Body")) {
                    registry.emplace<RigidBodyComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
                if (!registry.all_of<BoxColliderComponent>(entity) && ImGui::MenuItem("Box Collider")) {
                    registry.emplace<BoxColliderComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
                if (!registry.all_of<SphereColliderComponent>(entity) && ImGui::MenuItem("Sphere Collider")) {
                    registry.emplace<SphereColliderComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
                if (!registry.all_of<LightComponent>(entity) && ImGui::MenuItem("Light")) {
                    registry.emplace<LightComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
                if (!registry.all_of<AudioComponent>(entity) && ImGui::MenuItem("Audio")) {
                    registry.emplace<AudioComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
                if (!registry.all_of<ParticleSystemComponent>(entity) && ImGui::MenuItem("Particle System")) {
                    registry.emplace<ParticleSystemComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace Genesis::Engine
