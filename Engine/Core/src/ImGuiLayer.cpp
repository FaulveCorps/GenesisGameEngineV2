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

    // Load a modern UI font (with platform-aware fallbacks).
    const float fontSize = 17.0f;
    const char* fontCandidates[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/SegoeUIVariable.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
#endif
        "Assets/fonts/Inter.ttf",
        "Assets/fonts/Inter-Regular.ttf",
        "Assets/fonts/Monoid.ttf"
    };

    ImFont* font = nullptr;
    for (const char* path : fontCandidates) {
        font = io.Fonts->AddFontFromFileTTF(path, fontSize);
        if (font) {
            std::cout << "Loaded editor font: " << path << std::endl;
            break;
        }
    }

    if (!font) {
        std::cerr << "Failed to load editor fonts. Falling back to default." << std::endl;
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
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace Genesis::Engine
