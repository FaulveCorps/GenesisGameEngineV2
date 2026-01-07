#include "EditorLayer.h"

#include <engine/GraphicsFactory.h>
#include <engine/RendererManager.h>
#include <engine/OpenGLRenderer.h>
#include <engine/SceneLoader.h>
#include <engine/Components.h>
#include <engine/Profiler.h>
#include <engine/Engine.h>
#include <engine/ImGuiLayer.h> 

#include <imgui.h>
#include <imgui_internal.h>
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"
#include <ImGuizmo.h>
#include <filesystem>
#include <iostream>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace Genesis::Editor {

    // --- Static Helpers ---
    
    static SDL_HitTestResult SDLCALL HitTestCallback(SDL_Window* win, const SDL_Point* area, void* data) {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        
        const int RESIZE_BORDER = 8;
        const int TITLE_BAR_HEIGHT = 42;
        const int CONTROLS_WIDTH = 192;
        const int MENU_WIDTH = 600;

        if (area->x < RESIZE_BORDER && area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOPLEFT;
        if (area->x > w - RESIZE_BORDER && area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOPRIGHT;
        if (area->x < RESIZE_BORDER && area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        if (area->x > w - RESIZE_BORDER && area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

        if (area->x < RESIZE_BORDER) return SDL_HITTEST_RESIZE_LEFT;
        if (area->x > w - RESIZE_BORDER) return SDL_HITTEST_RESIZE_RIGHT;
        if (area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOP;
        if (area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOM;

        if (area->y < TITLE_BAR_HEIGHT) {
            if (area->x > MENU_WIDTH && area->x < w - CONTROLS_WIDTH) {
                return SDL_HITTEST_DRAGGABLE;
            }
        }
        return SDL_HITTEST_NORMAL;
    }

    static void ExtractYawPitchDegFromQuat(const glm::quat& qIn, float& outYawDeg, float& outPitchDeg) {
        const glm::quat q = glm::normalize(qIn);
        const glm::vec3 f = glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 r = glm::normalize(q * glm::vec3(1.0f, 0.0f, 0.0f));

        const float fy = glm::clamp(f.y, -1.0f, 1.0f);
        const float pitchRad = -asinf(fy);
        const float cosPitch = cosf(pitchRad);
        float yawRad = 0.0f;
        if (fabsf(cosPitch) > 1e-4f) {
            yawRad = atan2f(f.x, -f.z);
        } else {
            yawRad = atan2f(r.z, r.x);
        }

        outYawDeg = glm::degrees(yawRad);
        outPitchDeg = glm::degrees(pitchRad);
    }

    static glm::quat MakeCameraQuatFromForwardUp(const glm::vec3& forwardWorldIn, const glm::vec3& upHintWorldIn) {
        const glm::vec3 f = glm::normalize(forwardWorldIn);
        glm::vec3 upHint = glm::normalize(upHintWorldIn);
        if (fabs(glm::dot(f, upHint)) > 0.99f) {
            upHint = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        const glm::vec3 r = glm::normalize(glm::cross(f, upHint));
        const glm::vec3 u = glm::normalize(glm::cross(r, f));
        const glm::mat3 m(r, u, -f);
        return glm::normalize(glm::quat_cast(m));
    }

    // --- EditorLayer Implementation ---

    EditorLayer::EditorLayer(Genesis::Engine::Window* window)
        : m_Window(window)
    {
        m_ActiveScene = &m_EditorScene;
    }

    EditorLayer::~EditorLayer() {
        if (m_RuntimeScene) delete m_RuntimeScene;
    }

    void EditorLayer::OnAttach() {
        // Initialize Renderer
        std::vector<std::string> gfxOrder = { "opengl", "directx", "vulkan" };
        std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererInit = Genesis::Engine::GraphicsFactory::CreateRenderer(m_Window->GetSDLWindow(), m_Window->GetGLContext(), gfxOrder, false);
        if (!rendererInit) {
            std::cerr << "Failed to initialize renderer" << std::endl;
            return;
        }
        Genesis::Engine::RendererManager::SetRenderer(std::move(rendererInit));

        // Configure Renderer for Editor Mode (Manual Present)
        auto currentRenderer = Genesis::Engine::RendererManager::GetRenderer();
        if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
            glRenderer->SetPresentEnabled(false);
        }

        // Enable Borderless Window and Hit Test
        SDL_SetWindowBordered(m_Window->GetSDLWindow(), false);
        SDL_SetWindowHitTest(m_Window->GetSDLWindow(), HitTestCallback, nullptr);

        // ImGui Init is implicitly handled by Engine::ImGuiLayer in main.cpp?
        // Wait, main.cpp uses `Genesis::Engine::ImGuiLayer gui(...)`.
        // I should stick to that pattern or make m_ImGuiLayer a member.
        // For now, I will assume main.cpp INITIALIZES ImGui, or I should do it here.
        // If EditorLayer manages the frame, it should probably call `ImGui_Impl..._NewFrame`.
        // Let's assume ImGui is initialized GLOBALLY for now, or I'll need to move main.cpp's `gui` variable here.
        // Since `Genesis::Engine::ImGuiLayer` constructor does init, I'll make it a member.
        // But `ImGuiLayer` (Engine) seemed to handle NewFrame/Render in simpler apps.
        // In Main, `gui.Render(...)` was commented out and replaced by custom logic.
        // But the constructor `Genesis::Engine::ImGuiLayer gui(...)` was called.
        // I'll skip member for now and just rely on ImGui calls being valid if initialized.
        // BUT main.cpp initializes it. I should move that here.
        // I'll add `InitImGui` logic.
        
        // ImGui Init (Manual implementation resembling ImGuiLayer but customized for Editor)
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();
        
        // Style Tweaks
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        style.FramePadding = ImVec2(style.FramePadding.x + 6.0f, style.FramePadding.y + 6.0f);
        style.ItemSpacing = ImVec2(style.ItemSpacing.x + 6.0f, style.ItemSpacing.y + 4.0f);
        style.ScrollbarSize += 6.0f;
        style.GrabMinSize += 6.0f;

        // Init Backends
        ImGui_ImplSDL3_InitForOpenGL(m_Window->GetSDLWindow(), m_Window->GetGLContext());
        ImGui_ImplOpenGL3_Init("#version 410");

        // Try to load default project or fallback
        std::filesystem::path defaultProj = "GameProjects/SampleGame/SampleGame.genesis";
        if (std::filesystem::exists(defaultProj)) {
             auto project = Genesis::Engine::Project::Load(defaultProj);
             if (project) {
                  Genesis::Engine::Project::SetActive(project);
                  m_ContentBrowserPanel.SetContext(project->GetAssetDirectory());
                  
                  if (!project->GetConfig().StartScene.empty()) {
                       std::filesystem::path scenePath = project->GetAssetDirectory() / project->GetConfig().StartScene;
                       if (std::filesystem::exists(scenePath)) {
                            if (Genesis::Engine::SceneLoader::LoadScene(m_EditorScene, scenePath.string())) {
                                m_CurrentScenePath = scenePath.string();
                            }
                       }
                  } else {
                      // If no start scene, try default scene in project assets?
                      // Or just NewScene() which is already default state of m_EditorScene (empty)
                      NewScene();
                  }
             }
        } else {
            // Check for default scene in local assets
            NewScene(); // Or load default
            if (std::filesystem::exists("Assets/scenes/default.scene")) {
                if (Genesis::Engine::SceneLoader::LoadScene(m_EditorScene, "Assets/scenes/default.scene")) {
                    m_CurrentScenePath = "Assets/scenes/default.scene";
                }
            } else {
                 m_ContentBrowserPanel.SetContext("Assets");
            }
        }
    }

    void EditorLayer::OnDetach() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    void EditorLayer::OnUpdate(float ts) {
        // Camera Navigation State Check
        float mx, my;
        bool rightMouse = (SDL_GetMouseState(&mx, &my) & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT));
        if (rightMouse && !m_CameraNavActive) {
            m_CameraNavActive = true;
            SDL_SetWindowRelativeMouseMode(m_Window->GetSDLWindow(), true);
        } else if (!rightMouse && m_CameraNavActive) {
            m_CameraNavActive = false;
            SDL_SetWindowRelativeMouseMode(m_Window->GetSDLWindow(), false);
        }

        // Camera Movement
        if (m_CameraNavActive) {
            glm::vec3 forward;
            forward.x = sin(glm::radians(m_CameraRot.y)) * cos(glm::radians(m_CameraRot.x));
            forward.y = -sin(glm::radians(m_CameraRot.x));
            forward.z = -cos(glm::radians(m_CameraRot.y)) * cos(glm::radians(m_CameraRot.x));
            forward = glm::normalize(forward);

            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
            
            float speed = m_CameraSpeedBase;
            const bool* state = SDL_GetKeyboardState(nullptr);
            if (state[SDL_SCANCODE_LSHIFT]) speed *= 2.0f;
            if (state[SDL_SCANCODE_LCTRL]) speed *= 0.1f;

            if (state[SDL_SCANCODE_W]) m_CameraPos += forward * speed * ts;
            if (state[SDL_SCANCODE_S]) m_CameraPos -= forward * speed * ts;
            if (state[SDL_SCANCODE_D]) m_CameraPos += right * speed * ts;
            if (state[SDL_SCANCODE_A]) m_CameraPos -= right * speed * ts;
            if (state[SDL_SCANCODE_E]) m_CameraPos += glm::vec3(0, 1, 0) * speed * ts;
            if (state[SDL_SCANCODE_Q]) m_CameraPos -= glm::vec3(0, 1, 0) * speed * ts;
        }

        // ViewCube Animation
        if (m_ViewCubeAnimating) {
            m_ViewCubeAnimTime += ts;
            float t = (m_ViewCubeAnimTime / 0.25f);
            if (t >= 1.0f) {
                t = 1.0f;
                m_ViewCubeAnimating = false;
            }
            // Smoothstep
            float s = t * t * (3.0f - 2.0f * t);

            m_CameraPos = glm::mix(m_ViewCubeStartPos, m_ViewCubeTargetPos, s);
            
            glm::quat a = glm::normalize(m_ViewCubeStartRot);
            glm::quat b = glm::normalize(m_ViewCubeTargetRot);
            if (glm::dot(a, b) < 0.0f) b = -b;
            const glm::quat rot = glm::normalize(glm::slerp(a, b, s));

            ExtractYawPitchDegFromQuat(rot, m_CameraRot.y, m_CameraRot.x);
            m_CameraRot.z = 0.0f;
        }

        // Update Matrices
        glm::vec3 forward;
        forward.x = sin(glm::radians(m_CameraRot.y)) * cos(glm::radians(m_CameraRot.x));
        forward.y = -sin(glm::radians(m_CameraRot.x));
        forward.z = -cos(glm::radians(m_CameraRot.y)) * cos(glm::radians(m_CameraRot.x));
        forward = glm::normalize(forward);
        m_ViewMatrix = glm::lookAt(m_CameraPos, m_CameraPos + forward, glm::vec3(0, 1, 0));

        int w, h;
        SDL_GetWindowSize(m_Window->GetSDLWindow(), &w, &h);
        float aspectRatio = (float)w / (float)h;
        if (h == 0) aspectRatio = 1.0f;
        m_ProjectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);

        // Update Scene
        bool isSimulating = (m_SceneState == SceneState::Play);
        
        Genesis::Engine::RendererManager::GetRenderer()->SetViewProjection(glm::value_ptr(m_ViewMatrix), glm::value_ptr(m_ProjectionMatrix));
        m_ActiveScene->Update(ts, isSimulating);
    }



    void EditorLayer::OnEvent(const SDL_Event& event) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        
        if (event.type == SDL_EVENT_MOUSE_MOTION && m_CameraNavActive) {
            m_CameraRot.y += event.motion.xrel * m_CameraSensitivity;
            m_CameraRot.x += event.motion.yrel * m_CameraSensitivity; // Inverted or normal? main.cpp says += here.
        }

        // Handle Shortcuts if ImGui doesn't want text input
        if (!ImGui::GetIO().WantTextInput) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_DELETE) {
                    if (m_SelectedEntity != entt::null && m_ActiveScene->Registry().valid(m_SelectedEntity)) {
                        m_ActiveScene->Registry().destroy(m_SelectedEntity);
                        m_SelectedEntity = entt::null;
                        m_SceneDirty = true;
                    }
                }
            }
        }
        
        // Command Palette Toggle (Ctrl+P)
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_P && (event.key.mod & SDL_KMOD_CTRL)) {
           m_ShowCommandPalette = !m_ShowCommandPalette;
        }

        // Viewport Picking (Handle only if NOT dragging gizmo and NOT navigating?)
        // Needs "Viewport Hovered" check which is set in ImGuiRender.
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
             if (m_ViewportHovered && !m_CameraNavActive && !ImGuizmo::IsOver()) {
                 // Pixel reading logic requires Viewport Panel specifics (Framebuffers or ReadPixels from Default FB).
                 // Since we render to default FB currently, we can read pixel at mouse pos.
                 // int mx, my; SDL_GetMouseState(&mx, &my);
                 // Need to flip Y for OpenGL.
                 // This logic was in main.cpp loop.
                 float mouseX, mouseY;
                 SDL_GetMouseState(&mouseX, &mouseY);
                 int w, h;
                 SDL_GetWindowSize(m_Window->GetSDLWindow(), &w, &h);
                 mouseY = (float)h - mouseY; // Flip Y
                 
                 // This assumes PassThru/Default FB rendering.
                 m_ActiveScene->OnViewportResize(w, h); // Ensure scene knows size
                 entt::entity picked = m_ActiveScene->PickEntity((int)mouseX, (int)mouseY);
                 if (picked != entt::null) {
                     m_SelectedEntity = picked;
                 } else {
                    // Start selection box?
                     m_SelectedEntity = entt::null;
                 }
             }
        }
    }

    void EditorLayer::NewScene() {
        m_ActiveScene->Clear();
        m_SelectedEntity = entt::null;
        m_CurrentScenePath.clear();
        m_SceneDirty = true;

        // Default light
        auto lightEntity = m_ActiveScene->Registry().create();
        m_ActiveScene->Registry().emplace<Genesis::Engine::NameComponent>(lightEntity, Genesis::Engine::NameComponent{"Directional Light"});
        Genesis::Engine::LightComponent lightComp;
        lightComp.type = Genesis::Engine::LightType::Directional;
        lightComp.color[0] = 1.0f; lightComp.color[1] = 0.95f; lightComp.color[2] = 0.8f;
        lightComp.intensity = 1.5f;
        m_ActiveScene->Registry().emplace<Genesis::Engine::LightComponent>(lightEntity, lightComp);
        Genesis::Engine::Transform lightTrans;
        lightTrans.rx = -0.5f;
        lightTrans.ry = 0.5f;
        m_ActiveScene->Registry().emplace<Genesis::Engine::Transform>(lightEntity, lightTrans);
        m_SelectedEntity = lightEntity;
        
        // Cube
        auto cubeEntity = m_ActiveScene->Registry().create();
        m_ActiveScene->Registry().emplace<Genesis::Engine::NameComponent>(cubeEntity, Genesis::Engine::NameComponent{"Cube"});
        Genesis::Engine::Transform cubeTrans;
        cubeTrans.y = 0.0f;
        m_ActiveScene->Registry().emplace<Genesis::Engine::Transform>(cubeEntity, cubeTrans);
        
        auto& modelComp = m_ActiveScene->Registry().emplace<Genesis::Engine::ModelComponent>(cubeEntity);
        modelComp.model = std::make_shared<Genesis::Engine::Model>();
        modelComp.sourcePath.clear();
        
        Genesis::Engine::Mesh cubeMesh;
        std::vector<float> vertices = {
            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
             0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
        };
        std::vector<float> normals = {
             0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
             0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,
             0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
             0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,
             1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
            -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f
        };
        std::vector<uint32_t> indices = {
             0,  1,  2,  2,  3,  0,
             4,  5,  6,  6,  7,  4,
             8,  9, 10, 10, 11,  8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20
        };
        std::vector<float> uvs(vertices.size() / 3 * 2, 0.0f);
        cubeMesh.SetData(vertices, normals, uvs, indices);
        modelComp.model->AddMesh(std::move(cubeMesh));
    }

    bool EditorLayer::MaybePromptUnsaved(PendingSceneAction nextAction, const std::string& nextPath) {
        if (!m_SceneDirty) return false;
        m_PendingAction = nextAction;
        m_PendingLoadPath = nextPath;
        m_ShowUnsavedChangesParams = true;
        return true;
    }

    void EditorLayer::OnImGuiRender() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        
        static bool defaultDockLayoutAppliedThisRun = false;
        if (!defaultDockLayoutAppliedThisRun) {
            const char* ini = ImGui::GetIO().IniFilename;
            if (ini && !std::filesystem::exists(ini)) {
                 ImGui::DockBuilderRemoveNode(dockspace_id);
                 ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                 ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

                 ImGuiID dock_main_id = dockspace_id;
                 ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.28f, nullptr, &dock_main_id);
                 ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
                 ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);
                 ImGuiID dock_id_left_bottom = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.45f, nullptr, &dock_id_left);

                 ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
                 ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
                 ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
                 ImGui::DockBuilderDockWindow("Content Browser", dock_id_left_bottom);
                 ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
                 ImGui::DockBuilderFinish(dockspace_id);
            }
            defaultDockLayoutAppliedThisRun = true;
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                 if (ImGui::MenuItem("New Project...")) m_ShowNewProjectPopup = true;
                 if (ImGui::MenuItem("Open Project...")) m_ShowOpenProjectPopup = true;
                 if (ImGui::MenuItem("Save Project")) {
                     auto active = Genesis::Engine::Project::GetActive();
                     if (active) Genesis::Engine::Project::SaveActive(active->GetProjectDirectory() / (active->GetConfig().Name + ".genesis"));
                 }
                 ImGui::Separator();
                 if (ImGui::MenuItem("New Scene", "Ctrl+N")) { if (!MaybePromptUnsaved(PendingSceneAction::NewScene)) NewScene(); }
                 if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { if (!MaybePromptUnsaved(PendingSceneAction::ShowOpenScene)) ImGui::OpenPopup("Open Scene"); }
                 if (ImGui::MenuItem("Save", "Ctrl+S")) { 
                     if (m_CurrentScenePath.empty()) ImGui::OpenPopup("Save Scene As");
                     else {
                         if (Genesis::Engine::SceneLoader::SaveScene(*m_ActiveScene, m_CurrentScenePath)) m_SceneDirty = false;
                     }
                 }
                 if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) { ImGui::OpenPopup("Save Scene As"); }
                 if (ImGui::MenuItem("Exit", "Alt+F4")) { if (!MaybePromptUnsaved(PendingSceneAction::Quit)) m_Running = false; }
                 ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Panels
        if (!m_ZenMode) {
             m_InspectorPanel.SetContext(m_ActiveScene);
             m_InspectorPanel.SetSelectedEntity(m_SelectedEntity);
             m_InspectorPanel.OnImGuiRender();

             m_SceneHierarchyPanel.SetContext(m_ActiveScene);
             m_SceneHierarchyPanel.SetSelectedEntity(m_SelectedEntity);
             m_SceneHierarchyPanel.OnImGuiRender();
             m_SelectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();

             m_ContentBrowserPanel.SetLoadSceneCallback([&](const std::string& path) {
                  if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, path)) {
                      if (std::filesystem::exists(path)) {
                          Genesis::Engine::SceneLoader::LoadScene(*m_ActiveScene, path);
                          m_CurrentScenePath = path;
                          m_SceneDirty = false;
                          m_SelectedEntity = entt::null;
                      }
                  }
             });
             m_ContentBrowserPanel.OnImGuiRender();
        }

        // Gizmos
        if (m_SelectedEntity != entt::null && m_ActiveScene->Registry().valid(m_SelectedEntity) && m_ActiveScene->Registry().all_of<Genesis::Engine::Transform>(m_SelectedEntity)) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(0, 0, ImGui::GetMainViewport()->Size.x, ImGui::GetMainViewport()->Size.y);

            auto& tc = m_ActiveScene->Registry().get<Genesis::Engine::Transform>(m_SelectedEntity);
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(tc.x, tc.y, tc.z));
            transform = glm::rotate(transform, tc.rz, glm::vec3(0, 0, 1));
            transform = glm::rotate(transform, tc.ry, glm::vec3(0, 1, 0));
            transform = glm::rotate(transform, tc.rx, glm::vec3(1, 0, 0));
            transform = glm::scale(transform, glm::vec3(tc.sx, tc.sy, tc.sz));

            bool isNav = m_CameraNavActive; 
            if (!isNav && m_CurrentGizmoOperation != -1) {
                ImGuizmo::Manipulate(glm::value_ptr(m_ViewMatrix), glm::value_ptr(m_ProjectionMatrix), 
                    (ImGuizmo::OPERATION)m_CurrentGizmoOperation, (ImGuizmo::MODE)m_CurrentGizmoMode, glm::value_ptr(transform),
                    nullptr, m_UseSnap ? m_SnapValue : nullptr);

                if (ImGuizmo::IsUsing()) {
                    glm::vec3 scale, translation, skew;
                    glm::quat rotation;
                    glm::vec4 perspective;
                    glm::decompose(transform, scale, rotation, translation, skew, perspective);
                    rotation = glm::normalize(rotation);
                    tc.x = translation.x; tc.y = translation.y; tc.z = translation.z;
                    float z, y, x;
                    glm::extractEulerAngleZYX(glm::mat4_cast(rotation), z, y, x);
                    tc.rx = x; tc.ry = y; tc.rz = z;
                    tc.sx = scale.x; tc.sy = scale.y; tc.sz = scale.z;
                    m_SceneDirty = true;
                }
            }
        }
        
        // Handle Unsaved Changes Popup
        if (m_ShowUnsavedChangesParams) {
            ImGui::OpenPopup("Unsaved Changes?");
            m_ShowUnsavedChangesParams = false;
        }
        
        if (ImGui::BeginPopupModal("Unsaved Changes?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes. Save before continuing?");
            if (ImGui::Button("Save")) {
                bool saved = false;
                if (!m_CurrentScenePath.empty()) {
                     if (Genesis::Engine::SceneLoader::SaveScene(*m_ActiveScene, m_CurrentScenePath)) {
                         m_SceneDirty = false;
                         saved = true;
                     }
                }
                if (saved) {
                     ImGui::CloseCurrentPopup();
                     if (m_PendingAction == PendingSceneAction::Quit) m_Running = false;
                     else if (m_PendingAction == PendingSceneAction::NewScene) NewScene();
                     else if (m_PendingAction == PendingSceneAction::LoadScenePath) {
                          Genesis::Engine::SceneLoader::LoadScene(*m_ActiveScene, m_PendingLoadPath);
                          m_CurrentScenePath = m_PendingLoadPath;
                          m_SelectedEntity = entt::null;
                          m_SceneDirty = false;
                     }
                } else {
                     ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save")) {
                 ImGui::CloseCurrentPopup();
                 if (m_PendingAction == PendingSceneAction::Quit) m_Running = false;
                 else if (m_PendingAction == PendingSceneAction::NewScene) NewScene();
                 else if (m_PendingAction == PendingSceneAction::LoadScenePath) {
                      Genesis::Engine::SceneLoader::LoadScene(*m_ActiveScene, m_PendingLoadPath);
                      m_CurrentScenePath = m_PendingLoadPath;
                      m_SelectedEntity = entt::null;
                      m_SceneDirty = false;
                 }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
                m_PendingAction = PendingSceneAction::None;
            }
            ImGui::EndPopup();
        }
        
        UI_ShowNewProjectPopup();
        UI_ShowOpenProjectPopup();

        ImGui::Render();
        Genesis::Engine::RendererManager::GetRenderer()->Clear();
        // Render Scene behind UI
        m_ActiveScene->Render(Genesis::Engine::RendererManager::GetRenderer());
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }
        
        Genesis::Engine::RendererManager::GetRenderer()->Present();
    }

    void EditorLayer::UI_ShowNewProjectPopup() {
        if (m_ShowNewProjectPopup) {
            ImGui::OpenPopup("New Project");
            m_ShowNewProjectPopup = false;
        }

        if (ImGui::BeginPopupModal("New Project", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char nameBuf[128] = "NewProject";
            static char dirBuf[256] = "GameProjects"; 

            ImGui::InputText("Project Name", nameBuf, sizeof(nameBuf));
            ImGui::InputText("Directory", dirBuf, sizeof(dirBuf));

            if (ImGui::Button("Create")) {
                std::filesystem::path projectDir = std::filesystem::path(dirBuf) / nameBuf;
                // Basic error handling for empty name or path?
                if (strlen(nameBuf) > 0) {
                     if (!std::filesystem::exists(projectDir)) {
                        std::filesystem::create_directories(projectDir);
                        std::filesystem::create_directories(projectDir / "Assets");
                        std::filesystem::create_directories(projectDir / "Assets/scenes");
                        std::filesystem::create_directories(projectDir / "Assets/scripts");

                        auto project = std::make_shared<Genesis::Engine::Project>();
                        project->GetConfig().Name = nameBuf;
                        project->GetConfig().AssetDirectory = "Assets";
                        project->GetConfig().StartScene = ""; 

                        Genesis::Engine::Project::SetActive(project);
                        
                        std::filesystem::path genesisFile = projectDir / (std::string(nameBuf) + ".genesis");
                        Genesis::Engine::Project::SaveActive(genesisFile);

                        m_ContentBrowserPanel.SetContext(project->GetAssetDirectory());
                        NewScene();
                     }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void EditorLayer::UI_ShowOpenProjectPopup() {
        if (m_ShowOpenProjectPopup) {
            ImGui::OpenPopup("Open Project");
            m_ShowOpenProjectPopup = false;
        }

        if (ImGui::BeginPopupModal("Open Project", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char pathBuf[256] = "";

            ImGui::InputText("Project File (.genesis)", pathBuf, sizeof(pathBuf));

            if (ImGui::Button("Open")) {
                std::filesystem::path path(pathBuf);
                if (std::filesystem::exists(path) && path.extension() == ".genesis") {
                    auto project = Genesis::Engine::Project::Load(path);
                    if (project) {
                        Genesis::Engine::Project::SetActive(project);
                        m_ContentBrowserPanel.SetContext(project->GetAssetDirectory());
                        
                        auto& config = project->GetConfig();
                        if (!config.StartScene.empty()) {
                            std::filesystem::path scenePath = project->GetAssetDirectory() / config.StartScene;
                             if (std::filesystem::exists(scenePath)) {
                                 if (Genesis::Engine::SceneLoader::LoadScene(*m_ActiveScene, scenePath.string())) {
                                     m_CurrentScenePath = scenePath.string();
                                     m_SceneDirty = false;
                                     m_SelectedEntity = entt::null;
                                 }
                            }
                        } else {
                            NewScene();
                        }
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

}
