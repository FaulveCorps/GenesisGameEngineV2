#define SDL_MAIN_HANDLED
#include <iostream>
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/Scene.h"
#include "engine/GraphicsFactory.h"
#include "engine/RendererManager.h"
#include "engine/Components.h"
#include "engine/Model.h"
#include "engine/Profiler.h"
#include "engine/ImGuiLayer.h"
#include "engine/SceneLoader.h"
#include "engine/ShaderRegistry.h"
#include "engine/TextureRegistry.h"
#include "engine/OpenGLRenderer.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "imgui_internal.h"

// SDL Hit Test Callback for Custom Title Bar
SDL_HitTestResult SDLCALL HitTestCallback(SDL_Window* win, const SDL_Point* area, void* data) {
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    
    const int RESIZE_BORDER = 8;
    const int TITLE_BAR_HEIGHT = 30;
    const int CONTROLS_WIDTH = 120; // Width for Min/Max/Close buttons
    const int MENU_WIDTH = 500;     // Approximate width for Menu items (File, Window, etc.)

    // Resize Borders
    if (area->x < RESIZE_BORDER && area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (area->x > w - RESIZE_BORDER && area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (area->x < RESIZE_BORDER && area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (area->x > w - RESIZE_BORDER && area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

    if (area->x < RESIZE_BORDER) return SDL_HITTEST_RESIZE_LEFT;
    if (area->x > w - RESIZE_BORDER) return SDL_HITTEST_RESIZE_RIGHT;
    if (area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOP;
    if (area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOM;

    // Title Bar Dragging
    if (area->y < TITLE_BAR_HEIGHT) {
        // Allow clicking on Menu Items (Left) and Controls (Right)
        // The empty space in the middle is draggable
        if (area->x > MENU_WIDTH && area->x < w - CONTROLS_WIDTH) {
            return SDL_HITTEST_DRAGGABLE;
        }
    }

    return SDL_HITTEST_NORMAL;
}

int main(int argc, char** argv) {
    std::cout << "GenesisEditor starting..." << std::endl;
    if (!Genesis::Engine::Init()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }

    Genesis::Engine::Window window;
    if (!window.Init("Genesis Editor", 1600, 900)) {
        std::cerr << "Failed to create window" << std::endl;
        Genesis::Engine::Shutdown();
        return -1;
    }

    // Initialize Renderer (Default to factory settings)
    // Explicitly prefer OpenGL for Editor for stability and ImGui compatibility
    std::vector<std::string> gfxOrder = { "opengl", "directx", "vulkan" };
    std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererInit = Genesis::Engine::GraphicsFactory::CreateRenderer(window.GetSDLWindow(), window.GetGLContext(), gfxOrder, false);
    if (!rendererInit) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return -1;
    }
    Genesis::Engine::RendererManager::SetRenderer(std::move(rendererInit));

    // Configure Renderer for Editor Mode (Manual Present)
    auto currentRenderer = Genesis::Engine::RendererManager::GetRenderer();
    if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
        glRenderer->SetPresentEnabled(false);
    }

    // Enable Borderless Window and Hit Test for Custom Title Bar
    SDL_SetWindowBordered(window.GetSDLWindow(), false);
    SDL_SetWindowHitTest(window.GetSDLWindow(), HitTestCallback, nullptr);

    // Editor State
    entt::entity selectedEntity = entt::null;
    bool showCommandPalette = false;
    bool zenMode = false;
    bool showGrid = true;
    char commandSearchBuffer[128] = "";
    int selectedCommandIndex = 0;

    // Scene / File State
    std::string currentScenePath;
    bool sceneDirty = false;
    bool requestResetLayout = false;
    bool showOpenSceneModal = false;
    bool showSaveAsSceneModal = false;
    bool showAboutModal = false;
    bool focusInspectorName = false;
    char scenePathBuffer[512] = "";

    // Unsaved changes workflow
    enum class PendingSceneAction {
        None,
        Quit,
        NewScene,
        ShowOpenScene,
        LoadScenePath
    };
    PendingSceneAction pendingAction = PendingSceneAction::None;
    std::string pendingScenePath;
    bool showUnsavedChangesModal = false;

    // Editor Camera State
    glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 5.0f);
    glm::vec3 cameraRot = glm::vec3(-20.0f, 0.0f, 0.0f); // Pitch, Yaw, Roll
    ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE currentGizmoMode = ImGuizmo::LOCAL;

    // View Cube (smooth camera transition)
    bool viewCubeAnimating = false;
    float viewCubeAnimTime = 0.0f;
    float viewCubeAnimDuration = 0.25f; // seconds
    glm::vec3 viewCubeStartPos(0.0f);
    glm::vec3 viewCubeTargetPos(0.0f);
    glm::quat viewCubeStartRot(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat viewCubeTargetRot(1.0f, 0.0f, 0.0f, 0.0f);

    // Create scene
    Genesis::Engine::Scene scene;

    // Load default scene or create empty
    if (!Genesis::Engine::SceneLoader::LoadScene(scene, "Assets/scenes/default.scene")) {
        std::cout << "Editor: default scene not found, creating empty scene..." << std::endl;
        currentScenePath.clear();
        sceneDirty = true;
        
        // Create a default light so we can see things
        auto lightEntity = scene.Registry().create();
        scene.Registry().emplace<Genesis::Engine::NameComponent>(lightEntity, Genesis::Engine::NameComponent{"Directional Light"});
        Genesis::Engine::LightComponent lightComp;
        lightComp.type = Genesis::Engine::LightType::Directional;
        lightComp.color[0] = 1.0f; lightComp.color[1] = 1.0f; lightComp.color[2] = 1.0f;
        lightComp.intensity = 1.0f;
        scene.Registry().emplace<Genesis::Engine::LightComponent>(lightEntity, lightComp);
        
        Genesis::Engine::Transform lightTrans;
        lightTrans.rx = -0.5f; 
        lightTrans.ry = 0.5f;
        scene.Registry().emplace<Genesis::Engine::Transform>(lightEntity, lightTrans);
        
        // Auto-select the light entity for testing gizmos
        selectedEntity = lightEntity;

        // Create a Cube Entity for visual reference
        auto cubeEntity = scene.Registry().create();
        scene.Registry().emplace<Genesis::Engine::NameComponent>(cubeEntity, Genesis::Engine::NameComponent{"Cube"});
        Genesis::Engine::Transform cubeTrans;
        cubeTrans.y = 0.0f;
        scene.Registry().emplace<Genesis::Engine::Transform>(cubeEntity, cubeTrans);
        
        auto modelComp = scene.Registry().emplace<Genesis::Engine::ModelComponent>(cubeEntity);
        modelComp.model = std::make_shared<Genesis::Engine::Model>();
        modelComp.sourcePath.clear();
        
        // Manually create a cube mesh
        Genesis::Engine::Mesh cubeMesh;
        std::vector<float> vertices = {
            // Front face
            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
            // Back face
            -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
            // Top face
            -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,
            // Bottom face
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
            // Right face
             0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
            // Left face
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
        };
        std::vector<float> normals = {
            // Front
             0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
            // Back
             0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,
            // Top
             0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            // Bottom
             0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,
            // Right
             1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
            // Left
            -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f
        };
        std::vector<uint32_t> indices = {
             0,  1,  2,  2,  3,  0, // Front
             4,  5,  6,  6,  7,  4, // Back
             8,  9, 10, 10, 11,  8, // Top
            12, 13, 14, 14, 15, 12, // Bottom
            16, 17, 18, 18, 19, 16, // Right
            20, 21, 22, 22, 23, 20  // Left
        };
        // Add dummy UVs
        std::vector<float> uvs(vertices.size() / 3 * 2, 0.0f);

        cubeMesh.SetData(vertices, normals, uvs, indices);
        
        // We need to access the private m_meshes of Model to add this mesh
        // But Model::m_meshes is private. 
        // We should probably add a method to Model to add a mesh, or just use a public method if available.
        // Checking Model.h... m_meshes is private.
        // Let's modify Model.h to allow adding a mesh manually or make m_meshes public/protected.
        // For now, I'll just modify Model.h to add `AddMesh(Mesh&& mesh)`.
        modelComp.model->AddMesh(std::move(cubeMesh));
    }
    else {
        currentScenePath = "Assets/scenes/default.scene";
        sceneDirty = false;
    }

    // Setup profiler and ImGui
    Genesis::Engine::Profiler profiler;
    Genesis::Engine::ImGuiLayer gui(window.GetSDLWindow(), window.GetGLContext());

    std::cout << "Editor initialized. Entering main loop..." << std::endl;

    uint64_t lastTime = SDL_GetPerformanceCounter();

    bool running = true;

    auto DoNewScene = [&]() {
        scene.Clear();
        selectedEntity = entt::null;
        currentScenePath.clear();
        sceneDirty = true;

        // Default light
        auto lightEntity = scene.Registry().create();
        scene.Registry().emplace<Genesis::Engine::NameComponent>(lightEntity, Genesis::Engine::NameComponent{"Directional Light"});
        Genesis::Engine::LightComponent lightComp;
        lightComp.type = Genesis::Engine::LightType::Directional;
        lightComp.color[0] = 1.0f; lightComp.color[1] = 0.95f; lightComp.color[2] = 0.8f;
        lightComp.intensity = 1.5f;
        scene.Registry().emplace<Genesis::Engine::LightComponent>(lightEntity, lightComp);
        Genesis::Engine::Transform lightTrans;
        lightTrans.rx = -0.5f;
        lightTrans.ry = 0.5f;
        scene.Registry().emplace<Genesis::Engine::Transform>(lightEntity, lightTrans);
        selectedEntity = lightEntity;
    };

    auto MaybePromptUnsaved = [&](PendingSceneAction action, const std::string& path = std::string()) {
        if (!sceneDirty) return false;
        pendingAction = action;
        pendingScenePath = path;
        showUnsavedChangesModal = true;
        return true;
    };

    auto RequestQuit = [&]() {
        if (!MaybePromptUnsaved(PendingSceneAction::Quit)) {
            running = false;
        }
    };

    while (running) {
        // Pump SDL events (and intercept OS quit/close requests so we can prompt for unsaved changes).
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                RequestQuit();
            }

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                // ImGui multi-viewport creates additional SDL windows; do not treat their close
                // requests as a request to exit the whole app.
                if (event.window.windowID == window.GetWindowID()) {
                    RequestQuit();
                }
            }
        }

        if (!running) break;

        uint64_t now = SDL_GetPerformanceCounter();
        double dt = (double)((now - lastTime) * 1000 / SDL_GetPerformanceFrequency()) / 1000.0;
        lastTime = now;

        // Clamp dt to avoid huge jumps (e.g. debugging)
        if (dt > 0.1) dt = 0.1;

        // Smooth camera animation toward view-cube target.
        if (viewCubeAnimating) {
            viewCubeAnimTime += (float)dt;
            float t = viewCubeAnimDuration > 0.0f ? (viewCubeAnimTime / viewCubeAnimDuration) : 1.0f;
            if (t >= 1.0f) {
                t = 1.0f;
                viewCubeAnimating = false;
            }
            // Smoothstep
            float s = t * t * (3.0f - 2.0f * t);

            cameraPos = glm::mix(viewCubeStartPos, viewCubeTargetPos, s);
            glm::quat rot = glm::slerp(viewCubeStartRot, viewCubeTargetRot, s);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(rot));
            cameraRot.x = euler.x;
            cameraRot.y = euler.y;
            cameraRot.z = euler.z;
        }

        profiler.BeginFrame();

        // Start ImGui frame
        gui.NewFrame();
        ImGuizmo::BeginFrame();

        // Global Shortcuts (Editor)
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P)) {
            showCommandPalette = !showCommandPalette;
            if (showCommandPalette) {
                memset(commandSearchBuffer, 0, sizeof(commandSearchBuffer));
                selectedCommandIndex = 0;
                ImGui::SetNextWindowFocus();
            }
        }

        if (!ImGui::GetIO().WantTextInput) {
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
                if (!MaybePromptUnsaved(PendingSceneAction::NewScene)) {
                    DoNewScene();
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
                if (!MaybePromptUnsaved(PendingSceneAction::ShowOpenScene)) {
                    showOpenSceneModal = true;
                    if (!currentScenePath.empty()) {
                        strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                    } else {
                        strncpy_s(scenePathBuffer, "Assets/scenes/default.scene", sizeof(scenePathBuffer) - 1);
                    }
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && !ImGui::GetIO().KeyShift) {
                if (currentScenePath.empty()) {
                    showSaveAsSceneModal = true;
                    strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                } else {
                    if (Genesis::Engine::SceneLoader::SaveScene(scene, currentScenePath)) {
                        sceneDirty = false;
                    }
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::GetIO().KeyShift) {
                showSaveAsSceneModal = true;
                if (!currentScenePath.empty()) {
                    strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                } else {
                    strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                }
            }
        }
        if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (selectedEntity != entt::null && scene.Registry().valid(selectedEntity)) {
                scene.Registry().destroy(selectedEntity);
                selectedEntity = entt::null;
                sceneDirty = true;
            }
        }

        // Render Editor UI (on top of scene)
        // gui.Render(profiler, &scene); // Replaced with custom editor layout below

        // Custom Editor Layout
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        static bool first_time = true;
        if (first_time) {
            first_time = false;
            
            // Check if layout already exists (e.g. from imgui.ini)
            // If not, build default layout
            if (!ImGui::DockBuilderGetNode(dockspace_id)) {
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id);
                ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
                
                // Split Left into Top (Hierarchy) and Bottom (Content Browser)
                ImGuiID dock_id_left_bottom = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.5f, nullptr, &dock_id_left);

                ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
                ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
                ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
                ImGui::DockBuilderDockWindow("Content Browser", dock_id_left_bottom);
                ImGui::DockBuilderDockWindow("Console", dock_id_bottom);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        if (requestResetLayout) {
            requestResetLayout = false;
            first_time = true;
            ImGui::DockBuilderRemoveNode(dockspace_id);
        }

        // Custom Title Bar (VS Code Style)
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 8)); // Taller bar
        if (ImGui::BeginMainMenuBar()) {
            // Icon / Title
            ImGui::Text("  Genesis  ");
            ImGui::Separator();

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                    if (!MaybePromptUnsaved(PendingSceneAction::NewScene)) {
                        DoNewScene();
                    }
                }
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                    if (!MaybePromptUnsaved(PendingSceneAction::ShowOpenScene)) {
                        showOpenSceneModal = true;
                        if (!currentScenePath.empty()) {
                            strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                        } else {
                            strncpy_s(scenePathBuffer, "Assets/scenes/default.scene", sizeof(scenePathBuffer) - 1);
                        }
                    }
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    if (currentScenePath.empty()) {
                        showSaveAsSceneModal = true;
                        strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                    } else {
                        if (Genesis::Engine::SceneLoader::SaveScene(scene, currentScenePath)) {
                            sceneDirty = false;
                        }
                    }
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    showSaveAsSceneModal = true;
                    if (!currentScenePath.empty()) {
                        strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                    } else {
                        strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { RequestQuit(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Toggle Zen Mode", "Ctrl+K Z", &zenMode)) {}
                if (ImGui::MenuItem("Toggle Grid", "G", &showGrid)) {}
                if (ImGui::MenuItem("Reset Layout")) { requestResetLayout = true; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Viewport");
                ImGui::MenuItem("Inspector");
                ImGui::MenuItem("Scene Hierarchy");
                ImGui::MenuItem("Content Browser");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) { showAboutModal = true; }
                ImGui::EndMenu();
            }

            // Window Controls (Right Aligned)
            float buttonWidth = 45.0f;
            float buttonHeight = ImGui::GetWindowHeight(); // Match menu bar height
            float controlsWidth = buttonWidth * 3;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - controlsWidth);
            
            static bool isCustomMaximized = false;
            static int restoreX = 0, restoreY = 0, restoreW = 1600, restoreH = 900;

            // Helper lambda for drawing custom window buttons
            auto DrawWindowButton = [&](const char* id, int type) -> bool {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 size = ImVec2(buttonWidth, buttonHeight);
                bool clicked = ImGui::InvisibleButton(id, size);
                bool hovered = ImGui::IsItemHovered();
                bool active = ImGui::IsItemActive();
                
                ImU32 bgColor = 0;
                ImU32 iconColor = IM_COL32(200, 200, 200, 255); // Light grey text

                if (type == 2) { // Close button
                    if (hovered) bgColor = IM_COL32(232, 17, 35, 255); // Red
                    if (active) bgColor = IM_COL32(153, 11, 23, 255); // Darker Red
                    if (hovered || active) iconColor = IM_COL32(255, 255, 255, 255); // White icon
                } else {
                    if (hovered) bgColor = IM_COL32(255, 255, 255, 30); // Subtle white overlay
                    if (active) bgColor = IM_COL32(255, 255, 255, 60);
                }

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (bgColor != 0)
                    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor);

                // Draw Icon (Centered)
                ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
                float iconSize = 10.0f; // Fixed size for crisp look
                float half = iconSize * 0.5f;
                
                if (type == 0) { // Minimize
                    drawList->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y), iconColor, 1.0f);
                }
                else if (type == 1) { // Maximize/Restore
                    bool isMaximized = isCustomMaximized || (SDL_GetWindowFlags(window.GetSDLWindow()) & SDL_WINDOW_MAXIMIZED);
                    if (isMaximized) {
                        // Restore icon (two overlapping squares)
                        float offset = 2.0f;
                        // Back square
                        drawList->AddRect(ImVec2(center.x - half + offset, center.y - half - offset), ImVec2(center.x + half + offset, center.y + half - offset), iconColor, 1.0f);
                        // Front square fill (to hide back line)
                        drawList->AddRectFilled(ImVec2(center.x - half - offset, center.y - half + offset), ImVec2(center.x + half - offset, center.y + half + offset), ImGui::GetColorU32(ImGuiCol_MenuBarBg)); 
                        // Front square border
                        drawList->AddRect(ImVec2(center.x - half - offset, center.y - half + offset), ImVec2(center.x + half - offset, center.y + half + offset), iconColor, 1.0f);
                    } else {
                        // Maximize icon (one square)
                        drawList->AddRect(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), iconColor, 1.0f);
                    }
                }
                else if (type == 2) { // Close (X)
                    drawList->AddLine(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), iconColor, 1.0f);
                    drawList->AddLine(ImVec2(center.x + half, center.y - half), ImVec2(center.x - half, center.y + half), iconColor, 1.0f);
                }

                return clicked;
            };

            if (DrawWindowButton("Min", 0)) { SDL_MinimizeWindow(window.GetSDLWindow()); }
            ImGui::SameLine(0, 0);
            if (DrawWindowButton("Max", 1)) { 
                if (SDL_GetWindowFlags(window.GetSDLWindow()) & SDL_WINDOW_MAXIMIZED) {
                    SDL_RestoreWindow(window.GetSDLWindow());
                    isCustomMaximized = false;
                }
                else if (isCustomMaximized) {
                    SDL_SetWindowPosition(window.GetSDLWindow(), restoreX, restoreY);
                    SDL_SetWindowSize(window.GetSDLWindow(), restoreW, restoreH);
                    isCustomMaximized = false;
                }
                else {
                    SDL_GetWindowPosition(window.GetSDLWindow(), &restoreX, &restoreY);
                    SDL_GetWindowSize(window.GetSDLWindow(), &restoreW, &restoreH);
                    
                    SDL_DisplayID displayID = SDL_GetDisplayForWindow(window.GetSDLWindow());
                    SDL_Rect usableBounds;
                    if (SDL_GetDisplayUsableBounds(displayID, &usableBounds)) {
                        SDL_SetWindowPosition(window.GetSDLWindow(), usableBounds.x, usableBounds.y);
                        SDL_SetWindowSize(window.GetSDLWindow(), usableBounds.w, usableBounds.h);
                        isCustomMaximized = true;
                    }
                }
            }
            ImGui::SameLine(0, 0);
            if (DrawWindowButton("Close", 2)) { RequestQuit(); }

            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar();

        // Viewport Window
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        ImVec2 viewportWindowPos = ImGui::GetWindowPos();
        ImVec2 viewportContentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 viewportTopLeft = ImVec2(viewportWindowPos.x + viewportContentMin.x, viewportWindowPos.y + viewportContentMin.y);
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        const bool viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // Avoid conflicts: don't let gizmo hotkeys fire while camera navigation is active.
        const bool cameraNavActive = viewportFocused && viewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::GetIO().WantTextInput;

        // Gizmo Shortcuts
        if (!cameraNavActive && !ImGui::GetIO().WantTextInput && !ImGuizmo::IsUsing()) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) currentGizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::SCALE;
        }

        if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_G)) {
            // Keep this scoped to viewport focus to avoid toggling grid while typing elsewhere.
            if (viewportFocused) showGrid = !showGrid;
        }

        // Camera Navigation (viewport-scoped)
        if (cameraNavActive) {
            viewCubeAnimating = false;

            float speed = 5.0f * (float)dt;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) speed *= 2.0f;

            glm::vec3 forward;
            forward.x = sin(glm::radians(cameraRot.y)) * cos(glm::radians(cameraRot.x));
            forward.y = -sin(glm::radians(cameraRot.x));
            forward.z = -cos(glm::radians(cameraRot.y)) * cos(glm::radians(cameraRot.x));
            forward = glm::normalize(forward);

            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

            if (ImGui::IsKeyDown(ImGuiKey_W)) cameraPos += forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_S)) cameraPos -= forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_A)) cameraPos -= right * speed;
            if (ImGui::IsKeyDown(ImGuiKey_D)) cameraPos += right * speed;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) cameraPos -= glm::vec3(0, 1, 0) * speed;
            if (ImGui::IsKeyDown(ImGuiKey_E)) cameraPos += glm::vec3(0, 1, 0) * speed;

            // Mouse Look
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            cameraRot.y -= delta.x * 0.1f; // Invert X for intuitive look
            cameraRot.x -= delta.y * 0.1f; // Invert Y
        }

        // Update Camera Matrices
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::rotate(view, glm::radians(cameraRot.x), glm::vec3(1, 0, 0));
        view = glm::rotate(view, glm::radians(cameraRot.y), glm::vec3(0, 1, 0));
        view = glm::translate(view, -cameraPos);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f);
        
        // Update Projection Aspect Ratio based on Viewport Size
        if (viewportSize.x > 0 && viewportSize.y > 0) {
            projection = glm::perspective(glm::radians(45.0f), viewportSize.x / viewportSize.y, 0.1f, 100.0f);
        }

        // Render scene (now that camera matrices are final)
        if (currentRenderer) {
            currentRenderer->BeginFrame();
            currentRenderer->SetViewProjection(glm::value_ptr(view), glm::value_ptr(projection));
        }

        scene.Update(dt);
        scene.Render(currentRenderer);

        if (currentRenderer) {
            currentRenderer->EndFrame(); // Renders scene to internal texture (no swap)
        }

        if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
            uint64_t texID = glRenderer->GetFinalTextureID();
            // Invert V for OpenGL texture in ImGui
            ImGui::Image((ImTextureID)texID, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
        }

        // Drag/drop onto the viewport: create entity from a model, or open a dropped .scene
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                const char* droppedPath = (const char*)payload->Data;
                if (droppedPath && droppedPath[0] != 0) {
                    std::filesystem::path p(droppedPath);
                    std::string ext = p.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

                    if (ext == ".scene") {
                        if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, p.string())) {
                            if (Genesis::Engine::SceneLoader::LoadScene(scene, p.string())) {
                                currentScenePath = p.string();
                                sceneDirty = false;
                                selectedEntity = entt::null;
                            }
                        }
                    } else if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                        auto e = scene.Registry().create();
                        scene.Registry().emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{p.stem().string()});
                        scene.Registry().emplace<Genesis::Engine::Transform>(e);
                        Genesis::Engine::ModelComponent mc;
                        mc.model = std::make_shared<Genesis::Engine::Model>();
                        mc.sourcePath = p.string();
                        if (mc.model->Load(mc.sourcePath)) {
                            scene.Registry().emplace<Genesis::Engine::ModelComponent>(e, mc);
                            selectedEntity = e;
                            sceneDirty = true;
                        } else {
                            scene.Registry().destroy(e);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Draw Grid
        if (showGrid) {
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);
            glm::mat4 identityMatrix = glm::mat4(1.0f);
            ImGuizmo::DrawGrid(glm::value_ptr(view), glm::value_ptr(projection), glm::value_ptr(identityMatrix), 100.f);
        }

        // View Manipulate (View Cube)
        const float viewManipulateSize = 128.0f;
        const float viewManipulatePad = 8.0f;
        ImVec2 viewManipulatePos = ImVec2(
            viewportTopLeft.x + viewportSize.x - viewManipulateSize - viewManipulatePad,
            viewportTopLeft.y + viewManipulatePad
        );

        // Track interaction with the view cube specifically (avoid reacting to the entity gizmo).
        static bool viewCubeCapturing = false;
        ImVec2 viewCubeMin = viewManipulatePos;
        ImVec2 viewCubeMax = ImVec2(viewManipulatePos.x + viewManipulateSize, viewManipulatePos.y + viewManipulateSize);
        bool viewCubeHovered = ImGui::IsMouseHoveringRect(viewCubeMin, viewCubeMax, false);
        if (viewCubeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            viewCubeCapturing = true;
        }
        bool applyViewCubeThisFrame = viewCubeCapturing;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            viewCubeCapturing = false;
        }

        glm::mat4 viewCopy = view; // Make a copy to pass to ViewManipulate
        ImGuizmo::ViewManipulate(glm::value_ptr(viewCopy), 5.0f, viewManipulatePos, ImVec2(viewManipulateSize, viewManipulateSize), 0x10101010);

        // Axis labels (X/Y/Z) around the cube (overlay, positioned based on current view orientation)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32 shadow = IM_COL32(0, 0, 0, 160);
            const ImU32 textFront = IM_COL32(235, 235, 235, 230);
            const ImU32 textBack = IM_COL32(170, 170, 170, 180);
            const ImVec2 center = ImVec2(viewManipulatePos.x + viewManipulateSize * 0.5f, viewManipulatePos.y + viewManipulateSize * 0.5f);

            // View matrix transforms World -> View (camera). Use its rotation part to estimate
            // where the world axes point on the view-cube overlay.
            const glm::mat3 viewRot = glm::mat3(view);

            auto DrawAxisLabel = [&](const char* label, const glm::vec3& worldAxis) {
                // Axis direction in view/camera space.
                const glm::vec3 axisView = viewRot * worldAxis;

                // Map to 2D (screen y down). Normalize to avoid huge/NaN offsets.
                glm::vec2 axis2(axisView.x, -axisView.y);
                float len = glm::length(axis2);
                if (len < 1e-6f) {
                    return;
                }
                axis2 /= len;

                // Place near the edge of the view cube.
                const float radius = viewManipulateSize * 0.42f;
                ImVec2 pos = ImVec2(center.x + axis2.x * radius, center.y + axis2.y * radius);

                // In OpenGL view space, points "in front" typically have negative Z.
                const bool frontFacing = (axisView.z < 0.0f);
                const ImU32 text = frontFacing ? textFront : textBack;

                dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), shadow, label);
                dl->AddText(pos, text, label);
            };

            DrawAxisLabel("X", glm::vec3(1.0f, 0.0f, 0.0f));
            DrawAxisLabel("Y", glm::vec3(0.0f, 1.0f, 0.0f));
            DrawAxisLabel("Z", glm::vec3(0.0f, 0.0f, 1.0f));
        }

        auto MatDifferent = [](const glm::mat4& a, const glm::mat4& b) {
            constexpr float eps = 1e-5f;
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    if (fabs(a[c][r] - b[c][r]) > eps) return true;
                }
            }
            return false;
        };

        // Smoothly move camera when the user is interacting with the view cube (and it actually changed the view).
        if (applyViewCubeThisFrame && MatDifferent(viewCopy, view)) {
            // Target camera from the view matrix ImGuizmo produced.
            glm::mat4 inverseTargetView = glm::inverse(viewCopy);
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(inverseTargetView, scale, rotation, translation, skew, perspective);

            // Start point = current camera transform (in the same space as the target)
            glm::mat4 inverseCurrentView = glm::inverse(view);
            glm::quat curRot;
            glm::vec3 curTrans;
            glm::decompose(inverseCurrentView, scale, curRot, curTrans, skew, perspective);

            viewCubeStartPos = curTrans;
            viewCubeStartRot = curRot;
            viewCubeTargetPos = translation;
            viewCubeTargetRot = rotation;
            viewCubeAnimTime = 0.0f;
            viewCubeAnimating = true;
        }

        // Gizmos
        if (selectedEntity != entt::null && scene.Registry().valid(selectedEntity) && scene.Registry().all_of<Genesis::Engine::Transform>(selectedEntity)) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);

            auto& tc = scene.Registry().get<Genesis::Engine::Transform>(selectedEntity);
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(tc.x, tc.y, tc.z));
            // Transform rotation is stored in radians.
            transform = glm::rotate(transform, tc.rx, glm::vec3(1, 0, 0));
            transform = glm::rotate(transform, tc.ry, glm::vec3(0, 1, 0));
            transform = glm::rotate(transform, tc.rz, glm::vec3(0, 0, 1));
            transform = glm::scale(transform, glm::vec3(tc.sx, tc.sy, tc.sz));

            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), currentGizmoOperation, currentGizmoMode, glm::value_ptr(transform));

            if (ImGuizmo::IsUsing()) {
                float matrixTranslation[3], matrixRotation[3], matrixScale[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), matrixTranslation, matrixRotation, matrixScale);
                
                tc.x = matrixTranslation[0]; tc.y = matrixTranslation[1]; tc.z = matrixTranslation[2];
                // ImGuizmo gives rotation in degrees; store radians to match engine.
                tc.rx = glm::radians(matrixRotation[0]);
                tc.ry = glm::radians(matrixRotation[1]);
                tc.rz = glm::radians(matrixRotation[2]);
                tc.sx = matrixScale[0]; tc.sy = matrixScale[1]; tc.sz = matrixScale[2];
                sceneDirty = true;
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();

        // Scene Hierarchy
        if (!zenMode) {
            ImGui::Begin("Scene Hierarchy");
            if (ImGui::Button("Create Entity")) {
                auto e = scene.Registry().create();
                scene.Registry().emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{"Entity " + std::to_string((uint32_t)e)});
                scene.Registry().emplace<Genesis::Engine::Transform>(e);
                selectedEntity = e;
                sceneDirty = true;
            }
            ImGui::Separator();

            // Rename popup state
            static entt::entity renameEntity = entt::null;
            static char renameBuf[128] = "";

            // F2 focuses rename for selected entity
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2)) {
                if (selectedEntity != entt::null && scene.Registry().valid(selectedEntity)) {
                    renameEntity = selectedEntity;
                    std::string label;
                    if (scene.Registry().any_of<Genesis::Engine::NameComponent>(selectedEntity)) {
                        const auto& nc = scene.Registry().get<Genesis::Engine::NameComponent>(selectedEntity);
                        label = nc.name.empty() ? ("Entity " + std::to_string((uint32_t)selectedEntity)) : nc.name;
                    } else {
                        label = "Entity " + std::to_string((uint32_t)selectedEntity);
                    }
                    strncpy_s(renameBuf, label.c_str(), sizeof(renameBuf) - 1);
                    ImGui::OpenPopup("Rename Entity");
                }
            }

            scene.Registry().each([&](auto entity) {
                std::string label;
                if (scene.Registry().any_of<Genesis::Engine::NameComponent>(entity)) {
                    const auto& nc = scene.Registry().get<Genesis::Engine::NameComponent>(entity);
                    label = nc.name.empty() ? ("Entity " + std::to_string((uint32_t)entity)) : nc.name;
                } else {
                    label = "Entity " + std::to_string((uint32_t)entity);
                }
                bool isSelected = (selectedEntity == entity);
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedEntity = entity;
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    renameEntity = entity;
                    std::string curName = label;
                    strncpy_s(renameBuf, curName.c_str(), sizeof(renameBuf) - 1);
                    ImGui::OpenPopup("Rename Entity");
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename", "F2")) {
                        renameEntity = entity;
                        std::string curName = label;
                        strncpy_s(renameBuf, curName.c_str(), sizeof(renameBuf) - 1);
                        ImGui::OpenPopup("Rename Entity");
                    }
                    if (ImGui::MenuItem("Duplicate")) {
                        auto dup = scene.Registry().create();
                        if (scene.Registry().any_of<Genesis::Engine::NameComponent>(entity)) {
                            auto nc = scene.Registry().get<Genesis::Engine::NameComponent>(entity);
                            if (!nc.name.empty()) nc.name += " Copy";
                            scene.Registry().emplace<Genesis::Engine::NameComponent>(dup, nc);
                        }
                        if (scene.Registry().any_of<Genesis::Engine::Transform>(entity)) {
                            scene.Registry().emplace<Genesis::Engine::Transform>(dup, scene.Registry().get<Genesis::Engine::Transform>(entity));
                        }
                        if (scene.Registry().any_of<Genesis::Engine::LightComponent>(entity)) {
                            scene.Registry().emplace<Genesis::Engine::LightComponent>(dup, scene.Registry().get<Genesis::Engine::LightComponent>(entity));
                        }
                        if (scene.Registry().any_of<Genesis::Engine::ModelComponent>(entity)) {
                            scene.Registry().emplace<Genesis::Engine::ModelComponent>(dup, scene.Registry().get<Genesis::Engine::ModelComponent>(entity));
                        }
                        selectedEntity = dup;
                        sceneDirty = true;
                    }
                    if (ImGui::MenuItem("Delete", "Del")) {
                        if (scene.Registry().valid(entity)) {
                            scene.Registry().destroy(entity);
                            if (selectedEntity == entity) selectedEntity = entt::null;
                            sceneDirty = true;
                        }
                    }
                    ImGui::EndPopup();
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            });

            if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Name:");
                ImGui::PushItemWidth(300.0f);
                ImGui::InputText("##rename", renameBuf, sizeof(renameBuf));
                ImGui::PopItemWidth();

                bool commit = ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter);
                ImGui::SameLine();
                bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

                if (commit) {
                    if (renameEntity != entt::null && scene.Registry().valid(renameEntity)) {
                        scene.Registry().emplace_or_replace<Genesis::Engine::NameComponent>(renameEntity, Genesis::Engine::NameComponent{std::string(renameBuf)});
                        sceneDirty = true;
                    }
                    renameEntity = entt::null;
                    ImGui::CloseCurrentPopup();
                }
                if (cancel) {
                    renameEntity = entt::null;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::End();
        }

        // Inspector
        if (!zenMode) {
            ImGui::Begin("Inspector");
            if (selectedEntity != entt::null && scene.Registry().valid(selectedEntity)) {
                // Name (persistent edit buffer)
                static entt::entity lastNameEditEntity = entt::null;
                static char nameEditBuf[256] = "";
                if (selectedEntity != lastNameEditEntity) {
                    std::string name;
                    if (scene.Registry().any_of<Genesis::Engine::NameComponent>(selectedEntity)) {
                        name = scene.Registry().get<Genesis::Engine::NameComponent>(selectedEntity).name;
                    } else {
                        name = "Entity " + std::to_string((uint32_t)selectedEntity);
                    }
                    strncpy_s(nameEditBuf, name.c_str(), sizeof(nameEditBuf) - 1);
                    lastNameEditEntity = selectedEntity;
                }
                if (focusInspectorName) {
                    ImGui::SetKeyboardFocusHere();
                    focusInspectorName = false;
                }
                if (ImGui::InputText("Name", nameEditBuf, sizeof(nameEditBuf))) {
                    scene.Registry().emplace_or_replace<Genesis::Engine::NameComponent>(selectedEntity, Genesis::Engine::NameComponent{std::string(nameEditBuf)});
                    sceneDirty = true;
                }
                ImGui::Text("Entity ID: %u", (uint32_t)selectedEntity);
                ImGui::Separator();

                if (scene.Registry().all_of<Genesis::Engine::Transform>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& tc = scene.Registry().get<Genesis::Engine::Transform>(selectedEntity);
                        if (ImGui::DragFloat3("Position", &tc.x, 0.1f)) sceneDirty = true;

                        // Show rotation in degrees, store radians.
                        float rotDeg[3] = { glm::degrees(tc.rx), glm::degrees(tc.ry), glm::degrees(tc.rz) };
                        if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f)) {
                            tc.rx = glm::radians(rotDeg[0]);
                            tc.ry = glm::radians(rotDeg[1]);
                            tc.rz = glm::radians(rotDeg[2]);
                            sceneDirty = true;
                        }

                        if (ImGui::DragFloat3("Scale", &tc.sx, 0.01f, 0.0f, 1000.0f)) sceneDirty = true;
                    }
                }

                if (scene.Registry().all_of<Genesis::Engine::LightComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& lc = scene.Registry().get<Genesis::Engine::LightComponent>(selectedEntity);
                        const char* types[] = { "Directional", "Point" };
                        int currentType = (int)lc.type;
                        if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
                            lc.type = (Genesis::Engine::LightType)currentType;
                            sceneDirty = true;
                        }
                        if (ImGui::ColorEdit3("Color", lc.color)) sceneDirty = true;
                        if (ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 100.0f)) sceneDirty = true;
                        if (lc.type == Genesis::Engine::LightType::Point) {
                            if (ImGui::DragFloat("Range", &lc.range, 0.1f, 0.0f, 1000.0f)) sceneDirty = true;
                        }
                    }
                }

                if (scene.Registry().all_of<Genesis::Engine::ModelComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& mc = scene.Registry().get<Genesis::Engine::ModelComponent>(selectedEntity);
                        ImGui::TextWrapped("Source: %s", mc.sourcePath.empty() ? "(unspecified)" : mc.sourcePath.c_str());

                        if (ImGui::Button("Reload") && mc.model && !mc.sourcePath.empty()) {
                            mc.model->Load(mc.sourcePath);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove")) {
                            scene.Registry().remove<Genesis::Engine::ModelComponent>(selectedEntity);
                            sceneDirty = true;
                        }

                        ImGui::TextUnformatted("Drag a model from Content Browser onto this panel to assign.");
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                                const char* droppedPath = (const char*)payload->Data;
                                if (droppedPath && droppedPath[0] != 0) {
                                    std::filesystem::path p(droppedPath);
                                    std::string ext = p.extension().string();
                                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                                    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                                        if (!mc.model) mc.model = std::make_shared<Genesis::Engine::Model>();
                                        mc.sourcePath = p.string();
                                        mc.model->Load(mc.sourcePath);
                                        sceneDirty = true;
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                }

                if (ImGui::Button("Add Component")) {
                    ImGui::OpenPopup("AddComponentPopup");
                }
                if (ImGui::BeginPopup("AddComponentPopup")) {
                    if (ImGui::MenuItem("Light")) {
                        if (!scene.Registry().all_of<Genesis::Engine::LightComponent>(selectedEntity)) {
                            scene.Registry().emplace<Genesis::Engine::LightComponent>(selectedEntity);
                            sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Model")) {
                        if (!scene.Registry().all_of<Genesis::Engine::ModelComponent>(selectedEntity)) {
                            Genesis::Engine::ModelComponent mc;
                            mc.model = std::make_shared<Genesis::Engine::Model>();
                            mc.sourcePath.clear();
                            scene.Registry().emplace<Genesis::Engine::ModelComponent>(selectedEntity, mc);
                            sceneDirty = true;
                        }
                    }
                    ImGui::EndPopup();
                }
            } else {
                ImGui::Text("Select an entity to view details.");
            }
            ImGui::End();
        }

        // Content Browser
        if (!zenMode) {
            ImGui::Begin("Content Browser");
            static std::filesystem::path contentDir = "Assets";
            static char contentSearch[128] = "";

            if (std::filesystem::exists(contentDir)) {
                // Toolbar
                if (contentDir != std::filesystem::path("Assets")) {
                    if (ImGui::Button("<")) {
                        contentDir = contentDir.parent_path();
                    }
                    ImGui::SameLine();
                }
                ImGui::TextWrapped("%s", contentDir.string().c_str());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200.0f);
                ImGui::InputTextWithHint("##contentSearch", "Search...", contentSearch, sizeof(contentSearch));
                ImGui::Separator();

                float padding = 16.0f;
                float thumbnailSize = 64.0f;
                float cellSize = thumbnailSize + padding;
                float panelWidth = ImGui::GetContentRegionAvail().x;
                int columnCount = (int)(panelWidth / cellSize);
                if (columnCount < 1) columnCount = 1;

                ImGui::Columns(columnCount, 0, false);

                std::vector<std::filesystem::directory_entry> entries;
                for (const auto& entry : std::filesystem::directory_iterator(contentDir)) {
                    entries.push_back(entry);
                }
                std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                    if (a.is_directory() != b.is_directory()) return a.is_directory() > b.is_directory();
                    return a.path().filename().string() < b.path().filename().string();
                });

                for (const auto& entry : entries) {
                    std::string path = entry.path().string();
                    std::string filename = entry.path().filename().string();

                    if (contentSearch[0] != 0) {
                        std::string fLower = filename;
                        std::string sLower = contentSearch;
                        std::transform(fLower.begin(), fLower.end(), fLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                        std::transform(sLower.begin(), sLower.end(), sLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                        if (fLower.find(sLower) == std::string::npos) continue;
                    }
                    
                    ImGui::PushID(filename.c_str());
                    // Placeholder for thumbnail
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::ImageButton(filename.c_str(), (ImTextureID)0, ImVec2(thumbnailSize, thumbnailSize));
                    ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if (entry.is_directory()) {
                            contentDir = entry.path();
                        } else {
                            std::string ext = entry.path().extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                            if (ext == ".scene") {
                                const std::string pathStr = entry.path().string();
                                if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, pathStr)) {
                                    if (Genesis::Engine::SceneLoader::LoadScene(scene, pathStr)) {
                                        currentScenePath = pathStr;
                                        sceneDirty = false;
                                        selectedEntity = entt::null;
                                    }
                                }
                            }
                        }
                    }

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", path.c_str(), path.length() + 1);
                        ImGui::EndDragDropSource();
                    }

                    ImGui::TextWrapped("%s", filename.c_str());
                    ImGui::NextColumn();
                    ImGui::PopID();
                }
                ImGui::Columns(1);
            }
            ImGui::End();
        }

        // Console/Log
        if (!zenMode) {
            ImGui::Begin("Console");
            ImGui::Text("Genesis Engine Initialized.");
            ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // Command Palette
        if (showCommandPalette) {
            ImGui::OpenPopup("Command Palette");
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600, 400));
        if (ImGui::BeginPopupModal("Command Palette", &showCommandPalette, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            
            // Search Bar
            ImGui::PushItemWidth(-1);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::InputText("##search", commandSearchBuffer, sizeof(commandSearchBuffer));
            ImGui::PopItemWidth();

            ImGui::Separator();

            // Commands List
            const char* commands[] = {
                "File: New Scene",
                "File: Open Scene...",
                "File: Save",
                "File: Save As...",
                "File: Exit",
                "View: Reset Layout",
                "View: Toggle Zen Mode",
                "Entity: Create New",
                "Entity: Rename Selected",
                "Entity: Delete Selected",
                "Window: Toggle Fullscreen",
                "Help: About"
            };

            ImGui::BeginChild("CommandList");
            for (int i = 0; i < IM_ARRAYSIZE(commands); i++) {
                // Simple filter
                if (commandSearchBuffer[0] != 0 && strstr(commands[i], commandSearchBuffer) == nullptr) continue;

                bool isSelected = (selectedCommandIndex == i);
                if (ImGui::Selectable(commands[i], isSelected)) {
                    // Execute Command
                    std::string cmd = commands[i];
                    if (cmd == "File: New Scene") {
                        if (!MaybePromptUnsaved(PendingSceneAction::NewScene)) {
                            DoNewScene();
                        }
                    }
                    else if (cmd == "File: Open Scene...") {
                        if (!MaybePromptUnsaved(PendingSceneAction::ShowOpenScene)) {
                            showOpenSceneModal = true;
                            if (!currentScenePath.empty()) {
                                strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                            } else {
                                strncpy_s(scenePathBuffer, "Assets/scenes/default.scene", sizeof(scenePathBuffer) - 1);
                            }
                        }
                    }
                    else if (cmd == "File: Save") {
                        if (currentScenePath.empty()) {
                            showSaveAsSceneModal = true;
                            strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                        } else {
                            if (Genesis::Engine::SceneLoader::SaveScene(scene, currentScenePath)) {
                                sceneDirty = false;
                            }
                        }
                    }
                    else if (cmd == "File: Save As...") {
                        showSaveAsSceneModal = true;
                        if (!currentScenePath.empty()) {
                            strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                        } else {
                            strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                        }
                    }
                    else if (cmd == "File: Exit") {
                        RequestQuit();
                    }
                    else if (cmd == "View: Toggle Zen Mode") {
                        zenMode = !zenMode;
                    }
                    else if (cmd == "View: Reset Layout") {
                        requestResetLayout = true;
                    }
                    else if (cmd == "Entity: Create New") {
                        auto e = scene.Registry().create();
                        scene.Registry().emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{"Entity " + std::to_string((uint32_t)e)});
                        scene.Registry().emplace<Genesis::Engine::Transform>(e);
                        selectedEntity = e;
                        sceneDirty = true;
                    }
                    else if (cmd == "Entity: Rename Selected") {
                        if (selectedEntity != entt::null && scene.Registry().valid(selectedEntity)) {
                            zenMode = false;
                            focusInspectorName = true;
                        }
                    }
                    else if (cmd == "Entity: Delete Selected") {
                        if (selectedEntity != entt::null && scene.Registry().valid(selectedEntity)) {
                            scene.Registry().destroy(selectedEntity);
                            selectedEntity = entt::null;
                            sceneDirty = true;
                        }
                    }
                    else if (cmd == "Window: Toggle Fullscreen") {
                        if (SDL_GetWindowFlags(window.GetSDLWindow()) & SDL_WINDOW_FULLSCREEN)
                            SDL_SetWindowFullscreen(window.GetSDLWindow(), 0);
                        else
                            SDL_SetWindowFullscreen(window.GetSDLWindow(), true);
                    }
                    else if (cmd == "Help: About") {
                        showAboutModal = true;
                    }
                    
                    showCommandPalette = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild();

            // Close on Escape
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showCommandPalette = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Open Scene modal
        if (showOpenSceneModal) {
            ImGui::OpenPopup("Open Scene");
        }
        if (ImGui::BeginPopupModal("Open Scene", &showOpenSceneModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Scene path:");
            ImGui::PushItemWidth(520.0f);
            ImGui::InputText("##openScenePath", scenePathBuffer, sizeof(scenePathBuffer));
            ImGui::PopItemWidth();
            ImGui::TextDisabled("Tip: double-click a .scene in Content Browser to open it.");

            if (ImGui::Button("Open") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, std::string(scenePathBuffer))) {
                    if (Genesis::Engine::SceneLoader::LoadScene(scene, scenePathBuffer)) {
                        currentScenePath = scenePathBuffer;
                        sceneDirty = false;
                        selectedEntity = entt::null;
                    }
                }
                showOpenSceneModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showOpenSceneModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Save As modal
        if (showSaveAsSceneModal) {
            ImGui::OpenPopup("Save Scene As");
        }
        if (ImGui::BeginPopupModal("Save Scene As", &showSaveAsSceneModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Save to path:");
            ImGui::PushItemWidth(520.0f);
            ImGui::InputText("##saveScenePath", scenePathBuffer, sizeof(scenePathBuffer));
            ImGui::PopItemWidth();
            if (ImGui::Button("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (Genesis::Engine::SceneLoader::SaveScene(scene, scenePathBuffer)) {
                    currentScenePath = scenePathBuffer;
                    sceneDirty = false;

                    // If a destructive action was pending, run it now.
                    if (pendingAction != PendingSceneAction::None) {
                        auto action = pendingAction;
                        auto p = pendingScenePath;
                        pendingAction = PendingSceneAction::None;
                        pendingScenePath.clear();
                        if (action == PendingSceneAction::Quit) {
                            running = false;
                        } else if (action == PendingSceneAction::NewScene) {
                            DoNewScene();
                        } else if (action == PendingSceneAction::ShowOpenScene) {
                            showOpenSceneModal = true;
                        } else if (action == PendingSceneAction::LoadScenePath) {
                            if (Genesis::Engine::SceneLoader::LoadScene(scene, p)) {
                                currentScenePath = p;
                                sceneDirty = false;
                                selectedEntity = entt::null;
                            }
                        }
                    }
                }
                showSaveAsSceneModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showSaveAsSceneModal = false;
                // If we got here via an unsaved-changes prompt, treat cancel as cancelling the pending action.
                if (pendingAction != PendingSceneAction::None) {
                    pendingAction = PendingSceneAction::None;
                    pendingScenePath.clear();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Unsaved changes modal
        if (showUnsavedChangesModal) {
            ImGui::OpenPopup("Unsaved Changes");
        }
        if (ImGui::BeginPopupModal("Unsaved Changes", &showUnsavedChangesModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("You have unsaved changes.");
            ImGui::TextUnformatted("Save before continuing?");
            ImGui::Separator();

            static bool lastSaveFailed = false;
            if (lastSaveFailed) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Save failed.");
            }

            if (ImGui::Button("Save")) {
                lastSaveFailed = false;
                bool shouldClose = false;

                if (currentScenePath.empty()) {
                    showSaveAsSceneModal = true;
                    strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                    shouldClose = true;
                } else {
                    if (Genesis::Engine::SceneLoader::SaveScene(scene, currentScenePath)) {
                        sceneDirty = false;

                        auto action = pendingAction;
                        auto p = pendingScenePath;
                        pendingAction = PendingSceneAction::None;
                        pendingScenePath.clear();

                        if (action == PendingSceneAction::Quit) {
                            running = false;
                        } else if (action == PendingSceneAction::NewScene) {
                            DoNewScene();
                        } else if (action == PendingSceneAction::ShowOpenScene) {
                            showOpenSceneModal = true;
                        } else if (action == PendingSceneAction::LoadScenePath) {
                            if (Genesis::Engine::SceneLoader::LoadScene(scene, p)) {
                                currentScenePath = p;
                                sceneDirty = false;
                                selectedEntity = entt::null;
                            }
                        }
                        shouldClose = true;
                    } else {
                        lastSaveFailed = true;
                    }
                }

                if (shouldClose) {
                    showUnsavedChangesModal = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save")) {
                lastSaveFailed = false;
                auto action = pendingAction;
                auto p = pendingScenePath;
                pendingAction = PendingSceneAction::None;
                pendingScenePath.clear();

                if (action == PendingSceneAction::Quit) {
                    // Explicitly discard changes.
                    sceneDirty = false;
                    running = false;
                } else if (action == PendingSceneAction::NewScene) {
                    DoNewScene();
                } else if (action == PendingSceneAction::ShowOpenScene) {
                    showOpenSceneModal = true;
                } else if (action == PendingSceneAction::LoadScenePath) {
                    if (Genesis::Engine::SceneLoader::LoadScene(scene, p)) {
                        currentScenePath = p;
                        sceneDirty = false;
                        selectedEntity = entt::null;
                    }
                }

                showUnsavedChangesModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                lastSaveFailed = false;
                pendingAction = PendingSceneAction::None;
                pendingScenePath.clear();
                showUnsavedChangesModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // About modal
        if (showAboutModal) {
            ImGui::OpenPopup("About Genesis Editor");
            showAboutModal = false;
        }
        if (ImGui::BeginPopupModal("About Genesis Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Genesis Editor");
            ImGui::Separator();
            ImGui::TextWrapped("Usability improvements: scene save/load, entity naming/rename, viewport-scoped camera controls, drag-and-drop model spawning, content browser navigation.");
            ImGui::TextWrapped("Renderer: %s", currentRenderer ? currentRenderer->GetName().c_str() : "(none)");
            if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Status bar
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            const float barH = 22.0f;
            ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - barH));
            ImGui::SetNextWindowSize(ImVec2(vp->Size.x, barH));
            ImGui::SetNextWindowViewport(vp->ID);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNavFocus;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
            if (ImGui::Begin("##StatusBar", nullptr, flags)) {
                const char* dirtyMark = sceneDirty ? "*" : "";
                std::string sceneLabel = currentScenePath.empty() ? std::string("(unsaved)") : currentScenePath;
                ImGui::Text("Scene: %s%s", sceneLabel.c_str(), dirtyMark);
                ImGui::SameLine();
                ImGui::TextDisabled(" | ");
                ImGui::SameLine();
                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                ImGui::SameLine();
                ImGui::TextDisabled(" | ");
                ImGui::SameLine();
                ImGui::Text("Renderer: %s", currentRenderer ? currentRenderer->GetName().c_str() : "none");
                ImGui::SameLine();
                ImGui::TextDisabled(" | ");
                ImGui::SameLine();
                ImGui::Text("Cam: (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        ImGui::Render();
        
        // Ensure we are rendering to the default framebuffer (the window)
        if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
            glRenderer->BindDefaultFramebuffer();
            glRenderer->Clear(0.1f, 0.12f, 0.15f, 1.0f); // Clear to dark grey
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific binding SDL_GL_MakeCurrent() performs a lazy context switch so the saving/restoring is not strictly necessary.)
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        if (currentRenderer) {
            currentRenderer->Present(); // Swap buffers
        }
        
        profiler.EndFrame();
    }

    if (auto cur = Genesis::Engine::RendererManager::GetRenderer()) cur->Shutdown();
    window.Shutdown();
    Genesis::Engine::Shutdown();
    return 0;
}
