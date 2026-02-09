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
#include <cstring>

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
    entt::entity lastAudioEntity = entt::null;
    char audioPathBuf[512] = "";
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

    ImGuiStyle& style = ImGui::GetStyle();
    style.DisabledAlpha = 0.6f;
    style.WindowPadding = ImVec2(10.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.60f, 0.64f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.09f, 0.12f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.24f, 0.31f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.08f, 0.10f, 0.75f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.41f, 0.52f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.61f, 1.00f, 0.70f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.21f, 0.26f, 0.36f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.27f, 0.38f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.61f, 1.00f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.35f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.35f, 0.61f, 1.00f, 0.18f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.35f, 0.61f, 1.00f, 0.40f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.35f, 0.61f, 1.00f, 0.70f);
    colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.35f, 0.61f, 1.00f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.70f, 0.74f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.35f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.35f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.45f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.61f, 1.00f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.35f, 0.61f, 1.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.35f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
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
        auto& registry = scene->Registry();

        ImGui::Begin("Scene Hierarchy");
        auto HasPrimaryCamera = [&]() -> bool {
            auto view = registry.view<CameraComponent>();
            for (auto entity : view) {
                if (view.get<CameraComponent>(entity).primary) {
                    return true;
                }
            }
            return false;
        };

        auto MakeCameraPrimary = [&](entt::entity primary) {
            auto view = registry.view<CameraComponent>();
            for (auto entity : view) {
                auto& cam = view.get<CameraComponent>(entity);
                cam.primary = (entity == primary);
            }
        };

        auto CreateEmptyEntity = [&]() {
            auto e = registry.create();
            registry.emplace<NameComponent>(e, NameComponent{"Entity " + std::to_string((uint32_t)e)});
            registry.emplace<Transform>(e);
            m_impl->selectedEntity = e;
            return e;
        };

        auto CreateNamedEntity = [&](const std::string& name) {
            auto e = registry.create();
            registry.emplace<NameComponent>(e, NameComponent{name});
            registry.emplace<Transform>(e);
            m_impl->selectedEntity = e;
            return e;
        };

        if (ImGui::Button("Create")) {
            ImGui::OpenPopup("CreateEntityPopup");
        }
        if (ImGui::BeginPopup("CreateEntityPopup")) {
            if (ImGui::MenuItem("Empty")) {
                CreateEmptyEntity();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Camera")) {
                const bool hasPrimary = HasPrimaryCamera();
                auto e = CreateNamedEntity(hasPrimary ? "Camera" : "Main Camera");
                auto& cam = registry.emplace<CameraComponent>(e);
                cam.primary = !hasPrimary;
                if (cam.primary) {
                    MakeCameraPrimary(e);
                }
                auto& t = registry.get<Transform>(e);
                t.z = 10.0f;
            }
            if (ImGui::MenuItem("Directional Light")) {
                auto e = CreateNamedEntity("Directional Light");
                LightComponent lc;
                lc.type = LightType::Directional;
                lc.color[0] = 1.0f; lc.color[1] = 0.95f; lc.color[2] = 0.8f;
                lc.intensity = 1.5f;
                registry.emplace<LightComponent>(e, lc);
                auto& t = registry.get<Transform>(e);
                t.rx = -0.5f;
                t.ry = 0.5f;
            }
            if (ImGui::MenuItem("Point Light")) {
                auto e = CreateNamedEntity("Point Light");
                LightComponent lc;
                lc.type = LightType::Point;
                lc.color[0] = 1.0f; lc.color[1] = 1.0f; lc.color[2] = 1.0f;
                lc.intensity = 1.0f;
                lc.range = 10.0f;
                registry.emplace<LightComponent>(e, lc);
                auto& t = registry.get<Transform>(e);
                t.y = 2.0f;
            }
            if (ImGui::MenuItem("Audio Source")) {
                auto e = CreateNamedEntity("Audio Source");
                registry.emplace<AudioComponent>(e);
            }
            if (ImGui::MenuItem("Particle System")) {
                auto e = CreateNamedEntity("Particle System");
                registry.emplace<ParticleSystemComponent>(e);
            }
            ImGui::EndPopup();
        }

        registry.each([&](auto entity) {
            std::string label;
            if (registry.any_of<NameComponent>(entity)) {
                const auto& nc = registry.get<NameComponent>(entity);
                label = nc.name.empty() ? ("Entity " + std::to_string((uint32_t)entity)) : nc.name;
            } else {
                label = "Entity " + std::to_string((uint32_t)entity);
            }
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

            DrawComponent<AudioComponent>("Audio", registry, entity, [&](auto& component) {
                if (entity != m_impl->lastAudioEntity) {
                    strncpy_s(m_impl->audioPathBuf, component.soundPath.c_str(), sizeof(m_impl->audioPathBuf) - 1);
                    m_impl->lastAudioEntity = entity;
                }
                if (ImGui::InputText("Sound Path", m_impl->audioPathBuf, sizeof(m_impl->audioPathBuf))) {
                    component.soundPath = m_impl->audioPathBuf;
                }
                ImGui::DragFloat("Volume", &component.volume, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Pitch", &component.pitch, 0.01f, 0.1f, 4.0f);
                ImGui::Checkbox("Loop", &component.loop);
                ImGui::Checkbox("Play On Awake", &component.playOnAwake);
                ImGui::Checkbox("Spatial", &component.spatial);
                ImGui::DragFloat("Min Distance", &component.minDistance, 0.1f, 0.0f, 1000.0f);
                ImGui::DragFloat("Max Distance", &component.maxDistance, 0.1f, 0.0f, 10000.0f);
            });

            DrawComponent<ParticleSystemComponent>("Particle System", registry, entity, [](auto& component) {
                ImGui::DragFloat("Duration", &component.duration, 0.1f, 0.0f, 100.0f);
                ImGui::Checkbox("Looping", &component.looping);
                ImGui::Checkbox("Play On Awake", &component.playOnAwake);
                ImGui::DragFloat("Start Lifetime", &component.startLifetime, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Start Speed", &component.startSpeed, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Start Size", &component.startSize, 0.01f, 0.0f, 100.0f);
                ImGui::ColorEdit4("Start Color", component.startColor);
                ImGui::DragFloat("Rate Over Time", &component.rateOverTime, 0.1f, 0.0f, 10000.0f);
                ImGui::DragFloat("Emitter Radius", &component.emitterRadius, 0.01f, 0.0f, 1000.0f);
            });

            ImGui::Separator();

            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("AddComponent");

            if (ImGui::BeginPopup("AddComponent")) {
                if (!registry.all_of<CameraComponent>(entity) && ImGui::MenuItem("Camera")) {
                    const bool hasPrimary = HasPrimaryCamera();
                    auto& cam = registry.emplace<CameraComponent>(entity);
                    cam.primary = !hasPrimary;
                    if (cam.primary) {
                        MakeCameraPrimary(entity);
                    }
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
