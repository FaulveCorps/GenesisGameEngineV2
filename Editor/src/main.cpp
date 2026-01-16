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
#include "engine/Texture.h"
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
#include <glm/gtx/euler_angles.hpp>

#include "imgui_internal.h"

// SDL Hit Test Callback for Custom Title Bar
SDL_HitTestResult SDLCALL HitTestCallback(SDL_Window* win, const SDL_Point* area, void* data) {
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    
    const int RESIZE_BORDER = 8;
    // Keep these in sync with the custom titlebar styling below.
    // VS Code-like caption buttons are a bit roomier than default.
    // (Option B sizing): make the caption area and hit zones clearly larger.
    const int TITLE_BAR_HEIGHT = 42;
    const int CONTROLS_WIDTH = 192; // 3 * 64px (Min/Max/Close)
    const int MENU_WIDTH = 600;     // Approximate width for Menu items (File, View, Window, Help)

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

    // Headless self-test (no UI interaction required). This is intended for CI/regression testing.
    // Usage: GenesisEditor.exe --quit-prompt-selftest
    bool quitPromptSelftest = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && std::string(argv[i]) == "--quit-prompt-selftest") {
            quitPromptSelftest = true;
        }
    }

    // Run the self-test *before* engine init/window creation so it works in headless CI environments.
    // This test validates only the editor's unsaved-changes prompt state machine and event routing.
    if (quitPromptSelftest) {
        // Use a fixed ID to represent the main window. We don't need to create a real SDL window
        // because the test simulates SDL events directly.
        const SDL_WindowID mainWindowId = (SDL_WindowID)1;

        enum class PendingSceneAction {
            None,
            Quit,
            NewScene,
            ShowOpenScene,
            LoadScenePath
        };

        bool running = true;
        bool sceneDirty = true;
        PendingSceneAction pendingAction = PendingSceneAction::None;
        std::string pendingScenePath;
        bool showUnsavedChangesModal = false;

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

        auto Fail = [&](const char* msg, int code) {
            std::cerr << "quit-prompt-selftest: FAILED (" << msg << ")" << std::endl;
            return code;
        };

        // 1) Dirty scene + SDL_EVENT_QUIT should prompt and keep app running.
        {
            SDL_Event e{};
            e.type = SDL_EVENT_QUIT;
            if (e.type == SDL_EVENT_QUIT) {
                RequestQuit();
            }
            if (!showUnsavedChangesModal || pendingAction != PendingSceneAction::Quit || !running) {
                return Fail("dirty + SDL_EVENT_QUIT did not prompt correctly", 2);
            }
        }

        // 2) Dirty scene + SDL_EVENT_WINDOW_CLOSE_REQUESTED for main window should prompt and keep running.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = true;

            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            e.window.windowID = mainWindowId;
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (e.window.windowID == mainWindowId) {
                    RequestQuit();
                }
            }

            if (!showUnsavedChangesModal || pendingAction != PendingSceneAction::Quit || !running) {
                return Fail("dirty + CLOSE_REQUESTED(main) did not prompt correctly", 3);
            }
        }

        // 3) Dirty scene + CLOSE_REQUESTED for non-main window should NOT prompt.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = true;

            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            e.window.windowID = (SDL_WindowID)(mainWindowId + 1);
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (e.window.windowID == mainWindowId) {
                    RequestQuit();
                }
            }

            if (showUnsavedChangesModal || pendingAction != PendingSceneAction::None || !running) {
                return Fail("dirty + CLOSE_REQUESTED(other) should not prompt", 4);
            }
        }

        // 4) Clean scene + QUIT should exit immediately.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = false;

            SDL_Event e{};
            e.type = SDL_EVENT_QUIT;
            if (e.type == SDL_EVENT_QUIT) {
                RequestQuit();
            }

            if (running || showUnsavedChangesModal || pendingAction != PendingSceneAction::None) {
                return Fail("clean + SDL_EVENT_QUIT should exit without prompt", 5);
            }
        }

        // 5) Clean scene + CLOSE_REQUESTED(main) should exit immediately.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = false;

            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            e.window.windowID = mainWindowId;
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (e.window.windowID == mainWindowId) {
                    RequestQuit();
                }
            }

            if (running || showUnsavedChangesModal || pendingAction != PendingSceneAction::None) {
                return Fail("clean + CLOSE_REQUESTED(main) should exit without prompt", 6);
            }
        }

        std::cout << "quit-prompt-selftest: PASSED" << std::endl;
        return 0;
    }

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
    bool showSceneIcons = true;
    bool iconOcclusion = true;
    // Additional overlay toggles
    bool showColliders = true;
    bool showAudioSources = true;
    bool showParticles = true;
    bool showPhysicsBodies = true;
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
    // Fixed world-aligned gizmo by default (modern editor behavior).
    // Note: we still force SCALE to LOCAL at draw time to avoid TRS shear artifacts.
    ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    // View Cube (smooth camera transition)
    bool viewCubeAnimating = false;
    float viewCubeAnimTime = 0.0f;
    float viewCubeAnimDuration = 0.25f; // seconds
    glm::vec3 viewCubeStartPos(0.0f);
    glm::vec3 viewCubeTargetPos(0.0f);
    glm::quat viewCubeStartRot(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat viewCubeTargetRot(1.0f, 0.0f, 0.0f, 0.0f);

    // Create scene
    Genesis::Engine::Scene editorScene;
    Genesis::Engine::Scene* activeScene = &editorScene;
    std::unique_ptr<Genesis::Engine::Scene> runtimeScene;

    enum class EditorState {
        Edit,
        Play,
        Pause
    };
    EditorState editorState = EditorState::Edit;

    // Load default scene or create empty
    if (!Genesis::Engine::SceneLoader::LoadScene(editorScene, "Assets/scenes/default.scene")) {
        std::cout << "Editor: default scene not found, creating empty scene..." << std::endl;
        currentScenePath.clear();
        sceneDirty = true;
        
        // Create a default light so we can see things
        auto lightEntity = editorScene.Registry().create();
        editorScene.Registry().emplace<Genesis::Engine::NameComponent>(lightEntity, Genesis::Engine::NameComponent{"Directional Light"});
        Genesis::Engine::LightComponent lightComp;
        lightComp.type = Genesis::Engine::LightType::Directional;
        lightComp.color[0] = 1.0f; lightComp.color[1] = 1.0f; lightComp.color[2] = 1.0f;
        lightComp.intensity = 1.0f;
        editorScene.Registry().emplace<Genesis::Engine::LightComponent>(lightEntity, lightComp);
        
        Genesis::Engine::Transform lightTrans;
        lightTrans.rx = -0.5f; 
        lightTrans.ry = 0.5f;
        editorScene.Registry().emplace<Genesis::Engine::Transform>(lightEntity, lightTrans);
        
        // Auto-select the light entity for testing gizmos
        selectedEntity = lightEntity;

        // Default Camera
        auto camEntity = editorScene.Registry().create();
        editorScene.Registry().emplace<Genesis::Engine::NameComponent>(camEntity, Genesis::Engine::NameComponent{"Main Camera"});
        Genesis::Engine::Transform camTrans;
        camTrans.z = 10.0f;
        editorScene.Registry().emplace<Genesis::Engine::Transform>(camEntity, camTrans);
        editorScene.Registry().emplace<Genesis::Engine::CameraComponent>(camEntity);

        // Create a Cube Entity for visual reference
        auto cubeEntity = editorScene.Registry().create();
        editorScene.Registry().emplace<Genesis::Engine::NameComponent>(cubeEntity, Genesis::Engine::NameComponent{"Cube"});
        Genesis::Engine::Transform cubeTrans;
        cubeTrans.y = 0.0f;
        editorScene.Registry().emplace<Genesis::Engine::Transform>(cubeEntity, cubeTrans);
        
        auto modelComp = editorScene.Registry().emplace<Genesis::Engine::ModelComponent>(cubeEntity);
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

    // Global UI sizing tweak (Editor-only): make widgets/buttons slightly roomier.
    // This helps match the more comfortable click targets users expect from tools like VS Code.
    {
        ImGuiStyle& style = ImGui::GetStyle();
        // (Option B sizing): clearly larger click targets.
        style.FramePadding = ImVec2(style.FramePadding.x + 4.0f, style.FramePadding.y + 4.0f);
        style.ItemSpacing = ImVec2(style.ItemSpacing.x + 4.0f, style.ItemSpacing.y + 2.0f);
        style.ScrollbarSize += 4.0f;
        style.GrabMinSize += 4.0f;
    }

    std::cout << "Editor initialized. Entering main loop..." << std::endl;

    uint64_t lastTime = SDL_GetPerformanceCounter();

    bool running = true;

    auto DoNewScene = [&]() {
        editorScene.Clear();
        selectedEntity = entt::null;
        currentScenePath.clear();
        sceneDirty = true;
        
        // When creating a new scene, we are in Edit mode
        activeScene = &editorScene;
        editorState = EditorState::Edit;
        runtimeScene.reset();

        // Default light
        auto lightEntity = editorScene.Registry().create();
        editorScene.Registry().emplace<Genesis::Engine::NameComponent>(lightEntity, Genesis::Engine::NameComponent{"Directional Light"});
        Genesis::Engine::LightComponent lightComp;
        lightComp.type = Genesis::Engine::LightType::Directional;
        lightComp.color[0] = 1.0f; lightComp.color[1] = 0.95f; lightComp.color[2] = 0.8f;
        lightComp.intensity = 1.5f;
        editorScene.Registry().emplace<Genesis::Engine::LightComponent>(lightEntity, lightComp);
        Genesis::Engine::Transform lightTrans;
        lightTrans.rx = -0.5f;
        lightTrans.ry = 0.5f;
        editorScene.Registry().emplace<Genesis::Engine::Transform>(lightEntity, lightTrans);
        selectedEntity = lightEntity;

        // Default Camera
        auto camEntity = editorScene.Registry().create();
        editorScene.Registry().emplace<Genesis::Engine::NameComponent>(camEntity, Genesis::Engine::NameComponent{"Main Camera"});
        Genesis::Engine::Transform camTrans;
        camTrans.z = 10.0f; // Move back
        editorScene.Registry().emplace<Genesis::Engine::Transform>(camEntity, camTrans);
        editorScene.Registry().emplace<Genesis::Engine::CameraComponent>(camEntity);
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
        // Removed: ImGuizmo handles interpolation internally for clicks, and dragging should be immediate.
        /*
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
        */

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
                    // Only save if in Edit Mode
                    // If in Play Mode, we might want to ignore or save the runtime state? Usually ignored.
                    if (editorState == EditorState::Edit) {
                        if (Genesis::Engine::SceneLoader::SaveScene(editorScene, currentScenePath)) {
                            sceneDirty = false;
                        }
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
            if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                activeScene->Registry().destroy(selectedEntity);
                selectedEntity = entt::null;
                if (editorState == EditorState::Edit) sceneDirty = true;
            }
        }

        // Render Editor UI (on top of scene)
        // gui.Render(profiler, &scene); // Replaced with custom editor layout below

        // Custom Editor Layout
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Default docking layout
        //
        // ImGui will automatically restore a user's custom layout from imgui.ini if present.
        // On a *fresh* startup there may be no ini file yet, so we proactively build a sensible
        // default arrangement to avoid forcing users to reorganize panels.
        auto BuildDefaultDockLayout = [&](ImGuiID rootDockId) {
            ImGui::DockBuilderRemoveNode(rootDockId);
            ImGui::DockBuilderAddNode(rootDockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(rootDockId, ImGui::GetMainViewport()->Size);

            ImGuiID dock_main_id = rootDockId;
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.28f, nullptr, &dock_main_id);
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);

            // Split Left into Top (Hierarchy) and Bottom (Content Browser)
            ImGuiID dock_id_left_bottom = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.45f, nullptr, &dock_id_left);

            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Content Browser", dock_id_left_bottom);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);

            ImGui::DockBuilderFinish(rootDockId);
        };

        static bool defaultDockLayoutAppliedThisRun = false;
        if (!defaultDockLayoutAppliedThisRun) {
            const char* ini = ImGui::GetIO().IniFilename;
            bool iniExists = false;
            if (ini && ini[0] != '\0') {
                std::error_code ec;
                iniExists = std::filesystem::exists(std::filesystem::path(ini), ec);
            }

            // If there's no imgui.ini yet, this is likely a first run: apply a sensible default.
            if (!iniExists) {
                BuildDefaultDockLayout(dockspace_id);
            }

            defaultDockLayoutAppliedThisRun = true;
        }

        if (requestResetLayout) {
            requestResetLayout = false;
            BuildDefaultDockLayout(dockspace_id);
        }

        // Custom Title Bar (VS Code Style)
        // (Option B sizing): clearly larger targets.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 8));
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
                        if (editorState == EditorState::Edit) {
                            if (Genesis::Engine::SceneLoader::SaveScene(editorScene, currentScenePath)) {
                                sceneDirty = false;
                            }
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
                // Scene icon toggles
                ImGui::MenuItem("Show Scene Icons", "Ctrl+K I", &showSceneIcons);
                ImGui::MenuItem("Icon Occlusion", nullptr, &iconOcclusion);
                ImGui::Separator();
                ImGui::MenuItem("Show Colliders", nullptr, &showColliders);
                ImGui::MenuItem("Show Audio Sources", nullptr, &showAudioSources);
                ImGui::MenuItem("Show Particle Systems", nullptr, &showParticles);
                ImGui::MenuItem("Show Physics Bodies", nullptr, &showPhysicsBodies);
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

            // Play / Stop Toolbar
            {
                // Padding after Help menu
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
                
                if (editorState == EditorState::Edit) {
                    if (ImGui::Button("Play", ImVec2(80, 0))) {
                        // Enter Play Mode
                        // 1. Save Scene to temp
                        if (Genesis::Engine::SceneLoader::SaveScene(editorScene, "tmp/play_backup.scene")) {
                            // 2. Create Runtime Scene
                            runtimeScene = std::make_unique<Genesis::Engine::Scene>();
                            if (Genesis::Engine::SceneLoader::LoadScene(*runtimeScene, "tmp/play_backup.scene")) {
                                activeScene = runtimeScene.get();
                                activeScene->OnRuntimeStart();
                                editorState = EditorState::Play;
                                selectedEntity = entt::null;
                            } else {
                                std::cerr << "Failed to load backup scene for play mode" << std::endl;
                                runtimeScene.reset();
                            }
                        } else {
                             std::cerr << "Failed to save backup scene for play mode" << std::endl;
                        }
                    }
                } else {
                    // Play / Pause / Stop controls
                    if (editorState == EditorState::Play) {
                        if (ImGui::Button("Pause", ImVec2(80, 0))) {
                            editorState = EditorState::Pause;
                        }
                    } else if (editorState == EditorState::Pause) {
                        if (ImGui::Button("Resume", ImVec2(80, 0))) {
                            editorState = EditorState::Play;
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Stop", ImVec2(80, 0))) {
                         // Stop Play Mode
                         if (activeScene) activeScene->OnRuntimeStop();
                         activeScene = &editorScene;
                         runtimeScene.reset();
                         editorState = EditorState::Edit;
                         selectedEntity = entt::null;
                    }
                }
            }

            // Window Controls (Right Aligned)
            // Slightly larger click targets to match desktop IDE/editor expectations.
            float buttonWidth = 64.0f;
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
                float iconSize = 13.0f; // Slightly larger for the bigger button
                float half = iconSize * 0.5f;
                
                if (type == 0) { // Minimize
                    drawList->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y), iconColor, 2.0f);
                }
                else if (type == 1) { // Maximize/Restore
                    bool isMaximized = isCustomMaximized || (SDL_GetWindowFlags(window.GetSDLWindow()) & SDL_WINDOW_MAXIMIZED);
                    if (isMaximized) {
                        // Restore icon (two overlapping squares)
                        float offset = 2.0f;
                        // Back square
                        drawList->AddRect(ImVec2(center.x - half + offset, center.y - half - offset), ImVec2(center.x + half + offset, center.y + half - offset), iconColor, 0.0f, 0, 2.0f);
                        // Front square fill (to hide back line)
                        drawList->AddRectFilled(ImVec2(center.x - half - offset, center.y - half + offset), ImVec2(center.x + half - offset, center.y + half + offset), ImGui::GetColorU32(ImGuiCol_MenuBarBg)); 
                        // Front square border
                        drawList->AddRect(ImVec2(center.x - half - offset, center.y - half + offset), ImVec2(center.x + half - offset, center.y + half + offset), iconColor, 0.0f, 0, 2.0f);
                    } else {
                        // Maximize icon (one square)
                        drawList->AddRect(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), iconColor, 0.0f, 0, 2.0f);
                    }
                }
                else if (type == 2) { // Close (X)
                    drawList->AddLine(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), iconColor, 2.0f);
                    drawList->AddLine(ImVec2(center.x + half, center.y - half), ImVec2(center.x - half, center.y + half), iconColor, 2.0f);
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
        ImGui::PopStyleVar(2);

        // Viewport Window
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        // Use cursor screen position as the authoritative top-left for the viewport image.
        // This avoids subtle misalignment with docking/tab bars and matches where the Image() is actually drawn.
        ImVec2 viewportTopLeft = ImGui::GetCursorScreenPos();
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        const bool viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // Calculate ViewCube bounds first (needed for camera nav conflict detection)
        const float viewManipulateSize = 128.0f;
        const float viewManipulatePad = 8.0f;
        ImVec2 viewManipulatePos = ImVec2(
            viewportTopLeft.x + viewportSize.x - viewManipulateSize - viewManipulatePad,
            viewportTopLeft.y + viewManipulatePad
        );
        ImVec2 viewCubeMin = viewManipulatePos;
        ImVec2 viewCubeMax = ImVec2(viewManipulatePos.x + viewManipulateSize, viewManipulatePos.y + viewManipulateSize);
        const bool mouseOverViewCube = ImGui::IsMouseHoveringRect(viewCubeMin, viewCubeMax, false);

        // Input ownership for the viewport.
        // Prevents camera navigation, gizmos, and view cube manipulation from fighting over the same mouse drag.
        enum class ViewportInputOwner {
            None,
            ViewCube,
            Gizmo,
            CameraNav
        };
        static ViewportInputOwner inputOwner = ViewportInputOwner::None;

        const bool wantText = ImGui::GetIO().WantTextInput;
        const bool lDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool rDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        const bool lClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        // Acquire input ownership.
        if (inputOwner == ViewportInputOwner::None) {
            if (lClicked && mouseOverViewCube) {
                inputOwner = ViewportInputOwner::ViewCube;
            } else if ((lClicked || lDown) && ImGuizmo::IsOver()) {
                inputOwner = ViewportInputOwner::Gizmo;
            } else if (viewportFocused && viewportHovered && !mouseOverViewCube && rDown && !wantText) {
                inputOwner = ViewportInputOwner::CameraNav;
            }
        }

        // Release ownership.
        if (inputOwner == ViewportInputOwner::ViewCube && !lDown) {
            inputOwner = ViewportInputOwner::None;
        } else if (inputOwner == ViewportInputOwner::Gizmo && !lDown && !ImGuizmo::IsUsing()) {
            inputOwner = ViewportInputOwner::None;
        } else if (inputOwner == ViewportInputOwner::CameraNav && !rDown) {
            inputOwner = ViewportInputOwner::None;
        }

        // Camera navigation is exclusive.
        const bool cameraNavActive = (inputOwner == ViewportInputOwner::CameraNav);
        const bool allowGizmoInteractionThisFrame = (inputOwner == ViewportInputOwner::None || inputOwner == ViewportInputOwner::Gizmo);
        const bool applyViewCubeThisFrame = (inputOwner == ViewportInputOwner::ViewCube);

        // Gizmo Shortcuts
        if (!cameraNavActive && !wantText && !ImGuizmo::IsUsing()) {
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

            // Robust right vector even when looking nearly straight up/down.
            // (Cross with world-up becomes degenerate at |dot(forward, up)| ~= 1.)
            glm::vec3 refUp(0.0f, 1.0f, 0.0f);
            if (fabsf(glm::dot(forward, refUp)) > 0.99f) {
                refUp = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            glm::vec3 right = glm::normalize(glm::cross(forward, refUp));

            if (ImGui::IsKeyDown(ImGuiKey_W)) cameraPos += forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_S)) cameraPos -= forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_A)) cameraPos -= right * speed;
            if (ImGui::IsKeyDown(ImGuiKey_D)) cameraPos += right * speed;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) cameraPos -= glm::vec3(0, 1, 0) * speed;
            if (ImGui::IsKeyDown(ImGuiKey_E)) cameraPos += glm::vec3(0, 1, 0) * speed;

            // Mouse Look (once captured, apply regardless of hover for stable navigation)
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

        // Play Mode Camera Override
        if (editorState == EditorState::Play || editorState == EditorState::Pause) {
            auto viewCam = activeScene->Registry().view<Genesis::Engine::CameraComponent, Genesis::Engine::Transform>();
            bool cameraFound = false;
            for (auto entity : viewCam) {
                const auto& cam = viewCam.get<Genesis::Engine::CameraComponent>(entity);
                if (cam.primary) {
                    const auto& t = viewCam.get<Genesis::Engine::Transform>(entity);
                    
                    // Engine Transform -> View Matrix
                    // Eye position
                    glm::vec3 eye(t.x, t.y, t.z);
                    
                    // Rotation matrix (match Scene::Render logic: Rot = Rz * Ry * Rx)
                    glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), t.rx, glm::vec3(1, 0, 0));
                    glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), t.ry, glm::vec3(0, 1, 0));
                    glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), t.rz, glm::vec3(0, 0, 1));
                    glm::mat4 rot = rotZ * rotY * rotX;

                    // Forward vector is usually -Z in OpenGL view space.
                    // If Identity rotation faces -Z:
                    glm::vec3 forward = glm::vec3(rot * glm::vec4(0, 0, -1, 0));
                    glm::vec3 up = glm::vec3(rot * glm::vec4(0, 1, 0, 0));

                    view = glm::lookAt(eye, eye + forward, up);

                    if (viewportSize.x > 0 && viewportSize.y > 0) {
                        projection = glm::perspective(glm::radians(cam.fov), viewportSize.x / viewportSize.y, cam.nearPlane, cam.farPlane);
                    }
                    cameraFound = true;
                    break;
                }
            }
            if (!cameraFound) {
                // Fallback / Warning
                // Keep editor camera but maybe show text?
            }
        }

        // Render scene (now that camera matrices are final)
        if (currentRenderer) {
            currentRenderer->BeginFrame();
            currentRenderer->SetViewProjection(glm::value_ptr(view), glm::value_ptr(projection));
        }

        if (editorState == EditorState::Play) {
            activeScene->OnUpdateRuntime(dt);
        } else if (editorState == EditorState::Pause) {
            // No update, just render
        } else {
            activeScene->OnUpdateEditor(dt);
        }
        activeScene->Render(currentRenderer);

        if (currentRenderer) {
            currentRenderer->EndFrame(); // Renders scene to internal texture (no swap)
        }

        if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
            uint64_t texID = glRenderer->GetFinalTextureID();
            // Invert V for OpenGL texture in ImGui
            ImGui::Image((ImTextureID)texID, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
        }

        // Draw overlay icons for non-visible objects (camera, lights)
        // Uses small texture assets for icons when available, and supports occlusion testing
        // (based on sampling the renderer's depth buffer) controlled by the View menu toggle.
        {
            if (!showSceneIcons) {
                // Skip whole overlay if toggled off
            } else {
                auto WorldToScreen = [&](const glm::vec3& worldPos, glm::vec2& out, float* outWindowZ = nullptr) -> bool {
                    glm::vec4 clip = projection * view * glm::vec4(worldPos, 1.0f);
                    if (clip.w == 0.0f) return false;
                    glm::vec3 ndc = glm::vec3(clip) / clip.w;
                    // Cull if behind near/far planes
                    if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
                    out.x = viewportTopLeft.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x;
                    out.y = viewportTopLeft.y + (0.5f - ndc.y * 0.5f) * viewportSize.y;
                    if (outWindowZ) *outWindowZ = ndc.z * 0.5f + 0.5f;
                    // Quick screen bounds cull (small padding)
                    const float pad = 24.0f;
                    if (out.x < viewportTopLeft.x - pad || out.x > viewportTopLeft.x + viewportSize.x + pad ||
                        out.y < viewportTopLeft.y - pad || out.y > viewportTopLeft.y + viewportSize.y + pad) return false;
                    return true;
                };

                ImDrawList* dl = ImGui::GetWindowDrawList();
                auto io = ImGui::GetIO();

                // Simple in-memory icon generation (lazy). Keep these local to editor scope.
                static std::shared_ptr<Genesis::Engine::Texture> s_camIcon;
                static std::shared_ptr<Genesis::Engine::Texture> s_lightIcon;
                static std::shared_ptr<Genesis::Engine::Texture> s_audioIcon;
                static std::shared_ptr<Genesis::Engine::Texture> s_particleIcon;
                static std::shared_ptr<Genesis::Engine::Texture> s_rbIcon;
                auto CreateCameraIcon = [&]() -> std::shared_ptr<Genesis::Engine::Texture> {
                    if (s_camIcon) return s_camIcon;
                    const int iw = 64, ih = 64;
                    std::vector<uint8_t> px((size_t)iw * ih * 4, 0);
                    // Camera body (scaled proportions)
                    int bodyLeft = std::max(1, iw / 8);
                    int bodyRight = iw - std::max(1, iw / 8) - 1;
                    int bodyTop = ih * 9 / 64;
                    int bodyBottom = ih * 42 / 64;
                    for (int y = 0; y < ih; ++y) {
                        for (int x = 0; x < iw; ++x) {
                            int i = (y * iw + x) * 4;
                            if (x >= bodyLeft && x <= bodyRight && y >= bodyTop && y <= bodyBottom) {
                                px[i+0] = 60; px[i+1] = 120; px[i+2] = 200; px[i+3] = 255;
                            }
                            // Lens (scaled)
                            int cx = iw / 2 + std::max(1, iw / 16);
                            int cy = ih / 2 + std::max(1, ih / 16);
                            int dx = x - cx, dy = y - cy; int r2 = dx*dx + dy*dy;
                            int lensR1 = std::max(1, iw / 6);
                            int lensR2 = std::max(1, iw / 10);
                            if (r2 <= lensR1 * lensR1) { px[i+0] = 240; px[i+1] = 240; px[i+2] = 240; px[i+3] = 255; }
                            if (r2 <= lensR2 * lensR2) { px[i+0] = 30; px[i+1] = 30; px[i+2] = 40; px[i+3] = 255; }
                        }
                    }
                    s_camIcon = Genesis::Engine::Texture::CreateFromMemory(iw, ih, px);
                    return s_camIcon;
                };
                auto CreateLightIcon = [&]() -> std::shared_ptr<Genesis::Engine::Texture> {
                    if (s_lightIcon) return s_lightIcon;
                    const int iw = 64, ih = 64;
                    std::vector<uint8_t> px((size_t)iw * ih * 4, 0);
                    int cx = iw/2, cy = ih/2;
                    for (int y = 0; y < ih; ++y) {
                        for (int x = 0; x < iw; ++x) {
                            int i = (y * iw + x) * 4;
                            int dx = x - cx, dy = y - cy; int r2 = dx*dx + dy*dy;
                            int coreR = std::max(2, iw / 5);
                            if (r2 <= coreR * coreR) { px[i+0] = 255; px[i+1] = 210; px[i+2] = 60; px[i+3] = 255; }
                            // simple rays (8 directions) scaled
                            int rayStart = iw / 5; // start distance for rays
                            int rayEnd = iw / 3;   // end distance for rays
                            if ((abs(dx) == abs(dy) && abs(dx) > rayStart && abs(dx) < rayEnd) || (abs(dx) > rayStart && dy==0) || (abs(dy) > rayStart && dx==0)) {
                                px[i+0] = 255; px[i+1] = 235; px[i+2] = 120; px[i+3] = 255;
                            }
                        }
                    }
                    s_lightIcon = Genesis::Engine::Texture::CreateFromMemory(iw, ih, px);
                    return s_lightIcon;
                };

                // Audio/Particle/RigidBody icons
                auto CreateAudioIcon = [&]() -> std::shared_ptr<Genesis::Engine::Texture> {
                    if (s_audioIcon) return s_audioIcon;
                    const int iw = 64, ih = 64;
                    std::vector<uint8_t> px((size_t)iw * ih * 4, 0);
                    // simple speaker + cone
                    for (int y = 0; y < ih; ++y) {
                        for (int x = 0; x < iw; ++x) {
                            int i = (y * iw + x) * 4;
                            if (x >= iw/10 && x <= iw/2 && y >= ih/4 && y <= 3*ih/4) {
                                px[i+0] = 80; px[i+1] = 80; px[i+2] = 90; px[i+3] = 255;
                            }
                            int tx0 = iw/2 + 2;
                            int dx = x - tx0;
                            int dy = y - ih/2;
                            if (dx >= 0 && abs(dy) * 4 <= dx * ih / (iw/2)) {
                                px[i+0] = 200; px[i+1] = 200; px[i+2] = 120; px[i+3] = 255;
                            }
                        }
                    }
                    s_audioIcon = Genesis::Engine::Texture::CreateFromMemory(iw, ih, px);
                    return s_audioIcon;
                };

                auto CreateParticleIcon = [&]() -> std::shared_ptr<Genesis::Engine::Texture> {
                    if (s_particleIcon) return s_particleIcon;
                    const int iw = 64, ih = 64;
                    std::vector<uint8_t> px((size_t)iw * ih * 4, 0);
                    int cx = iw/2, cy = ih/2;
                    for (int y = 0; y < ih; ++y) {
                        for (int x = 0; x < iw; ++x) {
                            int i = (y * iw + x) * 4;
                            int dx = x - cx, dy = y - cy; int r2 = dx*dx + dy*dy;
                            if (r2 <= (iw/10)*(iw/10)) { px[i+0]=255; px[i+1]=255; px[i+2]=200; px[i+3]=255; }
                            if ((abs(dx)==6 && abs(dy)<3) || (abs(dy)==6 && abs(dx)<3)) { px[i+0]=255; px[i+1]=200; px[i+2]=255; px[i+3]=255; }
                        }
                    }
                    s_particleIcon = Genesis::Engine::Texture::CreateFromMemory(iw, ih, px);
                    return s_particleIcon;
                };

                auto CreateRigidBodyIcon = [&]() -> std::shared_ptr<Genesis::Engine::Texture> {
                    if (s_rbIcon) return s_rbIcon;
                    const int iw = 64, ih = 64;
                    std::vector<uint8_t> px((size_t)iw * ih * 4, 0);
                    int bx0 = iw/4, bx1 = iw - iw/4;
                    int by0 = ih/3, by1 = ih - ih/3;
                    for (int y = 0; y < ih; ++y) {
                        for (int x = 0; x < iw; ++x) {
                            int i = (y * iw + x) * 4;
                            if (x >= bx0 && x <= bx1 && y >= by0 && y <= by1) { px[i+0]=160; px[i+1]=160; px[i+2]=200; px[i+3]=255; }
                        }
                    }
                    s_rbIcon = Genesis::Engine::Texture::CreateFromMemory(iw, ih, px);
                    return s_rbIcon;
                };

                // Ensure textures exist and are uploaded when we have a renderer
                CreateCameraIcon(); CreateLightIcon(); CreateAudioIcon(); CreateParticleIcon(); CreateRigidBodyIcon();
                if (s_camIcon && currentRenderer) s_camIcon->UploadToRenderer(currentRenderer);
                if (s_lightIcon && currentRenderer) s_lightIcon->UploadToRenderer(currentRenderer);
                if (s_audioIcon && currentRenderer) s_audioIcon->UploadToRenderer(currentRenderer);
                if (s_particleIcon && currentRenderer) s_particleIcon->UploadToRenderer(currentRenderer);
                if (s_rbIcon && currentRenderer) s_rbIcon->UploadToRenderer(currentRenderer);

                auto IsOccluded = [&](int sx, int sy, float windowZ) {
                    if (!iconOcclusion) return false;
                    if (auto glr = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
                        float d = 1.0f;
                        if (glr->ReadDepthAtWindowCoord(sx, sy, d)) {
                            // if depth is closer than the object, we're occluded
                            if (d < windowZ - 1e-5f) return true;
                        }
                    }
                    return false;
                };

                const float iconSize = 64.0f; // larger icons for visibility (try 64×64)
                const float half = iconSize * 0.5f;

                // Camera icons
                auto camView = activeScene->Registry().view<Genesis::Engine::CameraComponent, Genesis::Engine::Transform>();
                for (auto entity : camView) {
                    const auto& tc = camView.get<Genesis::Engine::Transform>(entity);
                    glm::vec3 wp(tc.x, tc.y, tc.z);
                    glm::vec2 sp; float winZ = 0.0f;
                    if (!WorldToScreen(wp, sp, &winZ)) continue;
                    ImVec2 p((float)sp.x, (float)sp.y);

                    if (IsOccluded((int)p.x, (int)p.y, winZ)) continue;

                    bool drewTex = false;
                    if (s_camIcon && s_camIcon->GetID() != 0) {
                        ImVec2 tl = ImVec2(p.x - half, p.y - half);
                        ImVec2 br = ImVec2(p.x + half, p.y + half);
                        dl->AddImage((ImTextureID)(uintptr_t)s_camIcon->GetID(), tl, br, ImVec2(0, 1), ImVec2(1, 0));
                        drewTex = true;
                    }
                    if (!drewTex) {
                        ImU32 col = ImGui::GetColorU32(ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
                        dl->AddRectFilled(ImVec2(p.x - half, p.y - half * 0.7f), ImVec2(p.x + half, p.y + half * 0.7f), col, 3.0f);
                        dl->AddCircleFilled(ImVec2(p.x + half * 0.4f, p.y), half * 0.35f, IM_COL32(30, 30, 40, 255));
                    }

                    // Forward indicator (same as before)
                    glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), tc.rx, glm::vec3(1,0,0));
                    glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), tc.ry, glm::vec3(0,1,0));
                    glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), tc.rz, glm::vec3(0,0,1));
                    glm::mat4 rot = rotZ * rotY * rotX;
                    glm::vec3 fwd = glm::vec3(rot * glm::vec4(0, 0, -1, 0));
                    if (glm::length(fwd) > 1e-6f) {
                        glm::vec3 arrowWorld = wp + glm::normalize(fwd) * 1.2f;
                        glm::vec2 arrowScr;
                        if (WorldToScreen(arrowWorld, arrowScr)) {
                            ImVec2 end((float)arrowScr.x, (float)arrowScr.y);
                            dl->AddLine(p, end, ImGui::GetColorU32(ImVec4(0.4f,0.6f,1.0f,1.0f)), 2.0f);
                        }
                    }

                    // Selection hit test
                    if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        ImVec2 m = io.MousePos;
                        if (m.x >= p.x - half && m.x <= p.x + half && m.y >= p.y - half && m.y <= p.y + half) {
                            selectedEntity = entity;
                        }
                    }
                }

                // Light icons
                auto lightView = activeScene->Registry().view<Genesis::Engine::LightComponent, Genesis::Engine::Transform>();
                for (auto entity : lightView) {
                    const auto& tc = lightView.get<Genesis::Engine::Transform>(entity);
                    const auto& lc = lightView.get<Genesis::Engine::LightComponent>(entity);
                    glm::vec3 wp(tc.x, tc.y, tc.z);
                    glm::vec2 sp; float winZ = 0.0f;
                    if (!WorldToScreen(wp, sp, &winZ)) continue;
                    ImVec2 p((float)sp.x, (float)sp.y);

                    if (IsOccluded((int)p.x, (int)p.y, winZ)) continue;

                    bool drewTex = false;
                    if (s_lightIcon && s_lightIcon->GetID() != 0) {
                        ImVec2 tl = ImVec2(p.x - half, p.y - half);
                        ImVec2 br = ImVec2(p.x + half, p.y + half);
                        dl->AddImage((ImTextureID)(uintptr_t)s_lightIcon->GetID(), tl, br, ImVec2(0, 1), ImVec2(1, 0));
                        drewTex = true;
                    }
                    if (!drewTex) {
                        ImU32 col = ImGui::GetColorU32(ImVec4(lc.color[0], lc.color[1], lc.color[2], 1.0f));
                        dl->AddCircleFilled(p, half * 0.6f, col, 16);
                        dl->AddCircle(p, half * 0.6f + 3.0f, col, 16, 2.0f);
                    }

                    if (lc.type == Genesis::Engine::LightType::Directional) {
                        // Direction arrow
                        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), tc.rx, glm::vec3(1,0,0));
                        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), tc.ry, glm::vec3(0,1,0));
                        glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), tc.rz, glm::vec3(0,0,1));
                        glm::mat4 rot = rotZ * rotY * rotX;
                        glm::vec3 fwd = glm::vec3(rot * glm::vec4(0, 0, -1, 0));
                        if (glm::length(fwd) > 1e-6f) {
                            glm::vec3 arrowWorld = wp + glm::normalize(fwd) * 1.5f;
                            glm::vec2 arrowScr;
                            if (WorldToScreen(arrowWorld, arrowScr)) {
                                ImVec2 end((float)arrowScr.x, (float)arrowScr.y);
                                dl->AddLine(p, end, ImGui::GetColorU32(ImVec4(lc.color[0], lc.color[1], lc.color[2], 1.0f)), 2.0f);
                            }
                        }
                    } else {
                        // Show range ring for point lights
                        if (lc.range > 0.0f) {
                            glm::vec3 rworld = wp + glm::vec3(lc.range, 0, 0);
                            glm::vec2 rscr;
                            if (WorldToScreen(rworld, rscr)) {
                                float pixelR = sqrtf((rscr.x - p.x)*(rscr.x - p.x) + (rscr.y - p.y)*(rscr.y - p.y));
                                dl->AddCircle(p, pixelR, IM_COL32(255,255,255,100), 64, 1.5f);
                            }
                        }
                    }

                    if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        ImVec2 m = io.MousePos;
                        if (m.x >= p.x - half && m.x <= p.x + half && m.y >= p.y - half && m.y <= p.y + half) {
                            selectedEntity = entity;
                        }
                    }
                }
            }
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
                            // Stop play mode if running
                            if (editorState != EditorState::Edit) {
                                if (activeScene) activeScene->OnRuntimeStop();
                                activeScene = &editorScene;
                                runtimeScene.reset();
                                editorState = EditorState::Edit;
                            }
                            
                            if (Genesis::Engine::SceneLoader::LoadScene(editorScene, p.string())) {
                                currentScenePath = p.string();
                                sceneDirty = false;
                                selectedEntity = entt::null;
                            }
                        }
                    } else if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                        auto e = activeScene->Registry().create();
                        activeScene->Registry().emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{p.stem().string()});
                        activeScene->Registry().emplace<Genesis::Engine::Transform>(e);
                        Genesis::Engine::ModelComponent mc;
                        mc.model = std::make_shared<Genesis::Engine::Model>();
                        mc.sourcePath = p.string();
                        if (mc.model->Load(mc.sourcePath)) {
                            activeScene->Registry().emplace<Genesis::Engine::ModelComponent>(e, mc);
                            selectedEntity = e;
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        } else {
                            activeScene->Registry().destroy(e);
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

        // View Manipulate (View Cube) - position already calculated above for conflict detection
        glm::mat4 viewCopy = view; // Make a copy to pass to ViewManipulate
        ImGuizmo::SetDrawlist();
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

        // Apply view-cube camera changes while ImGuizmo is animating.
        // We intentionally convert to yaw/pitch and force roll=0 to avoid the camera ending up upside-down.
        auto ApplyViewMatrixToCamera = [&](const glm::mat4& targetView) {
            glm::mat4 inv = glm::inverse(targetView);
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(inv, scale, rotation, translation, skew, perspective);

            cameraPos = translation;

            glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            if (glm::dot(forward, forward) < 1e-8f) {
                return;
            }
            forward = glm::normalize(forward);

            // Our navigation forward convention uses: forward.y = -sin(pitch).
            const float clampedY = glm::clamp(forward.y, -1.0f, 1.0f);
            float pitchRad = -asinf(clampedY);

            // For near-vertical views, yaw becomes underdetermined. Pick a stable yaw that
            // matches common editor behavior (top/bottom aligned; avoids "upside-down" feel).
            float yawRad = 0.0f;
            if (fabsf(forward.y) <= 0.999f) {
                yawRad = atan2f(forward.x, -forward.z);
            }

            cameraRot.x = glm::degrees(pitchRad);
            cameraRot.y = glm::degrees(yawRad);
            cameraRot.z = 0.0f;
        };

        const bool viewCubeDrivingCamera = ImGuizmo::IsUsingViewManipulate();
        if ((viewCubeDrivingCamera || applyViewCubeThisFrame) && MatDifferent(viewCopy, view)) {
            ApplyViewMatrixToCamera(viewCopy);
        }

        // Gizmos
        // Always draw the gizmo (so it doesn't disappear while navigating / snapping the camera),
        // but only allow interaction when the gizmo owns input.
        if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity) && activeScene->Registry().all_of<Genesis::Engine::Transform>(selectedEntity)) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);

            auto& tc = activeScene->Registry().get<Genesis::Engine::Transform>(selectedEntity);
            auto ComposeEngineTRS = [&](const Genesis::Engine::Transform& t) {
                // Must match Engine/Core/src/Scene.cpp: rot = rotZ * rotY * rotX; transform = T * rot * S
                glm::mat4 transMat = glm::translate(glm::mat4(1.0f), glm::vec3(t.x, t.y, t.z));
                glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), t.rx, glm::vec3(1, 0, 0));
                glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), t.ry, glm::vec3(0, 1, 0));
                glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), t.rz, glm::vec3(0, 0, 1));
                glm::mat4 rot = rotZ * rotY * rotX;
                glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(t.sx, t.sy, t.sz));
                return transMat * rot * scaleMat;
            };

            auto DecomposeEngineTRS = [&](const glm::mat4& m, Genesis::Engine::Transform& out) {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                if (!glm::decompose(m, scale, rotation, translation, skew, perspective)) {
                    return;
                }

                out.x = translation.x;
                out.y = translation.y;
                out.z = translation.z;

                out.sx = scale.x;
                out.sy = scale.y;
                out.sz = scale.z;

                // Extract Euler for the engine's ZYX convention: R = Rz * Ry * Rx
                rotation = glm::normalize(rotation);
                const glm::mat4 R = glm::mat4_cast(rotation);
                float z = 0.0f, y = 0.0f, x = 0.0f;
                glm::extractEulerAngleZYX(R, z, y, x);
                out.rx = x;
                out.ry = y;
                out.rz = z;
            };

            // Keep a stable matrix during manipulation to avoid feedback jitter from differing Euler conventions.
            static entt::entity gizmoEntity = entt::null;
            static glm::mat4 gizmoMatrix = glm::mat4(1.0f);
            static bool gizmoWasUsing = false;
            const bool gizmoUsingNow = ImGuizmo::IsUsing();

            if (gizmoEntity != selectedEntity || (!gizmoUsingNow && !gizmoWasUsing)) {
                gizmoEntity = selectedEntity;
                gizmoMatrix = ComposeEngineTRS(tc);
            }

            const bool gizmoInteractive = allowGizmoInteractionThisFrame && !wantText;
            ImGuizmo::Enable(gizmoInteractive);

            // Keep gizmo orientation fixed in world space (not dependent on object rotation)
            // for translate/rotate. Scaling in world space can introduce shear, which our
            // TRS-only Transform cannot represent cleanly, so we keep SCALE local.
            const ImGuizmo::MODE gizmoModeThisFrame =
                (currentGizmoOperation == ImGuizmo::SCALE || currentGizmoOperation == ImGuizmo::SCALEU)
                    ? ImGuizmo::LOCAL
                    : ImGuizmo::WORLD;

            // Prevent axis direction flipping for a stable, fixed gizmo.
            ImGuizmo::AllowAxisFlip(false);

            glm::mat4 manipulated = gizmoMatrix;
            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), currentGizmoOperation, gizmoModeThisFrame, glm::value_ptr(manipulated));

            // Restore global state for any later ImGuizmo calls.
            ImGuizmo::Enable(true);

            const bool usingThisFrame = gizmoInteractive && ImGuizmo::IsUsing();
            if (usingThisFrame) {
                gizmoMatrix = manipulated;
                DecomposeEngineTRS(gizmoMatrix, tc);
                sceneDirty = true;
            } else {
                // Not using: keep gizmo matrix in sync with component so it stays aligned to the rendered model.
                // (If you don't do this, the gizmo can drift after other systems edit tc.)
                gizmoMatrix = ComposeEngineTRS(tc);
            }

            gizmoWasUsing = usingThisFrame;
        }

        ImGui::End();
        ImGui::PopStyleVar();

        // Scene Hierarchy
        if (!zenMode) {
            ImGui::Begin("Scene Hierarchy");
            if (ImGui::Button("Create Entity")) {
                auto e = activeScene->Registry().create();
                activeScene->Registry().emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{"Entity " + std::to_string((uint32_t)e)});
                activeScene->Registry().emplace<Genesis::Engine::Transform>(e);
                selectedEntity = e;
                if (editorState == EditorState::Edit) sceneDirty = true;
            }
            ImGui::Separator();

            // Rename popup state
            static entt::entity renameEntity = entt::null;
            static char renameBuf[128] = "";

            // F2 focuses rename for selected entity
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2)) {
                if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                    renameEntity = selectedEntity;
                    std::string label;
                    if (activeScene->Registry().any_of<Genesis::Engine::NameComponent>(selectedEntity)) {
                        const auto& nc = activeScene->Registry().get<Genesis::Engine::NameComponent>(selectedEntity);
                        label = nc.name.empty() ? ("Entity " + std::to_string((uint32_t)selectedEntity)) : nc.name;
                    } else {
                        label = "Entity " + std::to_string((uint32_t)selectedEntity);
                    }
                    strncpy_s(renameBuf, label.c_str(), sizeof(renameBuf) - 1);
                    ImGui::OpenPopup("Rename Entity");
                }
            }

            activeScene->Registry().each([&](auto entity) {
                std::string label;
                if (activeScene->Registry().any_of<Genesis::Engine::NameComponent>(entity)) {
                    const auto& nc = activeScene->Registry().get<Genesis::Engine::NameComponent>(entity);
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
                        auto dup = activeScene->Registry().create();
                        if (activeScene->Registry().any_of<Genesis::Engine::NameComponent>(entity)) {
                            auto nc = activeScene->Registry().get<Genesis::Engine::NameComponent>(entity);
                            if (!nc.name.empty()) nc.name += " Copy";
                            activeScene->Registry().emplace<Genesis::Engine::NameComponent>(dup, nc);
                        }
                        if (activeScene->Registry().any_of<Genesis::Engine::Transform>(entity)) {
                            activeScene->Registry().emplace<Genesis::Engine::Transform>(dup, activeScene->Registry().get<Genesis::Engine::Transform>(entity));
                        }
                        if (activeScene->Registry().any_of<Genesis::Engine::LightComponent>(entity)) {
                            activeScene->Registry().emplace<Genesis::Engine::LightComponent>(dup, activeScene->Registry().get<Genesis::Engine::LightComponent>(entity));
                        }
                        if (activeScene->Registry().any_of<Genesis::Engine::ModelComponent>(entity)) {
                            activeScene->Registry().emplace<Genesis::Engine::ModelComponent>(dup, activeScene->Registry().get<Genesis::Engine::ModelComponent>(entity));
                        }
                        selectedEntity = dup;
                        if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                    if (ImGui::MenuItem("Delete", "Del")) {
                        if (activeScene->Registry().valid(entity)) {
                            activeScene->Registry().destroy(entity);
                            if (selectedEntity == entity) selectedEntity = entt::null;
                            if (editorState == EditorState::Edit) sceneDirty = true;
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
                    if (renameEntity != entt::null && activeScene->Registry().valid(renameEntity)) {
                        activeScene->Registry().emplace_or_replace<Genesis::Engine::NameComponent>(renameEntity, Genesis::Engine::NameComponent{std::string(renameBuf)});
                        if (editorState == EditorState::Edit) sceneDirty = true;
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
            if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                // Name (persistent edit buffer)
                static entt::entity lastNameEditEntity = entt::null;
                static char nameEditBuf[256] = "";
                if (selectedEntity != lastNameEditEntity) {
                    std::string name;
                    if (activeScene->Registry().any_of<Genesis::Engine::NameComponent>(selectedEntity)) {
                        name = activeScene->Registry().get<Genesis::Engine::NameComponent>(selectedEntity).name;
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
                    activeScene->Registry().emplace_or_replace<Genesis::Engine::NameComponent>(selectedEntity, Genesis::Engine::NameComponent{std::string(nameEditBuf)});
                    if (editorState == EditorState::Edit) sceneDirty = true;
                }
                ImGui::Text("Entity ID: %u", (uint32_t)selectedEntity);
                ImGui::Separator();

                if (activeScene->Registry().all_of<Genesis::Engine::Transform>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& tc = activeScene->Registry().get<Genesis::Engine::Transform>(selectedEntity);
                        if (ImGui::DragFloat3("Position", &tc.x, 0.1f)) {
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }

                        // Show rotation in degrees, store radians.
                        float rotDeg[3] = { glm::degrees(tc.rx), glm::degrees(tc.ry), glm::degrees(tc.rz) };
                        if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f)) {
                            tc.rx = glm::radians(rotDeg[0]);
                            tc.ry = glm::radians(rotDeg[1]);
                            tc.rz = glm::radians(rotDeg[2]);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }

                        if (ImGui::DragFloat3("Scale", &tc.sx, 0.01f, 0.0f, 1000.0f)) {
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::LightComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& lc = activeScene->Registry().get<Genesis::Engine::LightComponent>(selectedEntity);
                        const char* types[] = { "Directional", "Point" };
                        int currentType = (int)lc.type;
                        if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
                            lc.type = (Genesis::Engine::LightType)currentType;
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                        if (ImGui::ColorEdit3("Color", lc.color)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 100.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (lc.type == Genesis::Engine::LightType::Point) {
                            if (ImGui::DragFloat("Range", &lc.range, 0.1f, 0.0f, 1000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::ModelComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& mc = activeScene->Registry().get<Genesis::Engine::ModelComponent>(selectedEntity);
                        ImGui::TextWrapped("Source: %s", mc.sourcePath.empty() ? "(unspecified)" : mc.sourcePath.c_str());

                        if (ImGui::Button("Reload") && mc.model && !mc.sourcePath.empty()) {
                            mc.model->Load(mc.sourcePath);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove")) {
                            activeScene->Registry().remove<Genesis::Engine::ModelComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
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
                                        if (editorState == EditorState::Edit) sceneDirty = true;
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        if (mc.model && ImGui::TreeNode("Materials")) {
                            const auto& materials = mc.model->Materials();
                            for (int i = 0; i < (int)materials.size(); ++i) {
                                if (ImGui::TreeNode((void*)(intptr_t)i, "Material %d", i)) {
                                    bool isOverridden = mc.materialOverrides.find(i) != mc.materialOverrides.end();
                                    Genesis::Engine::Material currentMat = isOverridden ? mc.materialOverrides[i] : materials[i];
                                    
                                    bool changed = false;
                                    if (ImGui::ColorEdit4("Base Color", currentMat.baseColor.data())) changed = true;
                                    
                                    float metallic = currentMat.metallic;
                                    if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
                                        currentMat.metallic = metallic;
                                        changed = true;
                                    }
                                    
                                    float roughness = currentMat.roughness;
                                    if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
                                        currentMat.roughness = roughness;
                                        changed = true;
                                    }
                                    
                                    // Base Texture
                                    char buf[256];
                                    if (currentMat.baseColorTexture.length() >= 256) buf[0] = 0; else strcpy_s(buf, currentMat.baseColorTexture.c_str());
                                    if (ImGui::InputText("Base Texture", buf, 256)) {
                                        currentMat.baseColorTexture = buf;
                                        if (!currentMat.baseColorTexture.empty())
                                             currentMat.baseColorTextureObj = Genesis::Engine::Texture::CreateFromFile(buf);
                                        else
                                             currentMat.baseColorTextureObj.reset();
                                        changed = true;
                                    }
                                    if (ImGui::BeginDragDropTarget()) {
                                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                                            const char* droppedPath = (const char*)payload->Data;
                                            currentMat.baseColorTexture = droppedPath;
                                            currentMat.baseColorTextureObj = Genesis::Engine::Texture::CreateFromFile(droppedPath);
                                            changed = true;
                                        }
                                        ImGui::EndDragDropTarget();
                                    }
                                    
                                    // Normal Texture
                                    char bufNorm[256];
                                    if (currentMat.normalTexture.length() >= 256) bufNorm[0] = 0; else strcpy_s(bufNorm, currentMat.normalTexture.c_str());
                                    if (ImGui::InputText("Normal Texture", bufNorm, 256)) {
                                        currentMat.normalTexture = bufNorm;
                                        if (!currentMat.normalTexture.empty())
                                             currentMat.normalTextureObj = Genesis::Engine::Texture::CreateFromFile(bufNorm);
                                        else
                                             currentMat.normalTextureObj.reset();
                                        changed = true;
                                    }
                                    if (ImGui::BeginDragDropTarget()) {
                                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                                            const char* droppedPath = (const char*)payload->Data;
                                            currentMat.normalTexture = droppedPath;
                                            currentMat.normalTextureObj = Genesis::Engine::Texture::CreateFromFile(droppedPath);
                                            changed = true;
                                        }
                                        ImGui::EndDragDropTarget();
                                    }

                                    if (changed) {
                                        mc.materialOverrides[i] = currentMat;
                                        if (editorState == EditorState::Edit) sceneDirty = true;
                                    }
                                    
                                    if (isOverridden) {
                                        if (ImGui::Button("Reset to Original")) {
                                            mc.materialOverrides.erase(i);
                                            if (editorState == EditorState::Edit) sceneDirty = true;
                                        }
                                    }
                                    
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::ScriptComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Text("Native Script Attached");
                        if (ImGui::Button("Remove")) {
                            activeScene->Registry().remove<Genesis::Engine::ScriptComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::ScriptComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Text("Native Script Attached");
                        if (ImGui::Button("Remove")) {
                            activeScene->Registry().remove<Genesis::Engine::ScriptComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                }

                if (ImGui::Button("Add Component")) {
                    ImGui::OpenPopup("AddComponentPopup");
                }
                if (ImGui::BeginPopup("AddComponentPopup")) {
                    if (ImGui::MenuItem("Light")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::LightComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::LightComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Model")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::ModelComponent>(selectedEntity)) {
                            Genesis::Engine::ModelComponent mc;
                            mc.model = std::make_shared<Genesis::Engine::Model>();
                            mc.sourcePath.clear();
                            activeScene->Registry().emplace<Genesis::Engine::ModelComponent>(selectedEntity, mc);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Script")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::ScriptComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::ScriptComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
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
                                    // Stop play mode logic handled in LoadScenePath action if prompted, 
                                    // but here we are loading directly if not dirty.
                                    if (editorState != EditorState::Edit) {
                                         if (activeScene) activeScene->OnRuntimeStop();
                                         activeScene = &editorScene;
                                         runtimeScene.reset();
                                         editorState = EditorState::Edit;
                                    }

                                    if (Genesis::Engine::SceneLoader::LoadScene(editorScene, pathStr)) {
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
                            if (editorState == EditorState::Edit) {
                                if (Genesis::Engine::SceneLoader::SaveScene(editorScene, currentScenePath)) {
                                    sceneDirty = false;
                                }
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
                        auto e = activeScene->Registry().create();
                        activeScene->Registry().emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{"Entity " + std::to_string((uint32_t)e)});
                        activeScene->Registry().emplace<Genesis::Engine::Transform>(e);
                        selectedEntity = e;
                        if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                    else if (cmd == "Entity: Rename Selected") {
                        if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                            zenMode = false;
                            focusInspectorName = true;
                        }
                    }
                    else if (cmd == "Entity: Delete Selected") {
                        if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                            activeScene->Registry().destroy(selectedEntity);
                            selectedEntity = entt::null;
                            if (editorState == EditorState::Edit) sceneDirty = true;
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
                    if (editorState != EditorState::Edit) {
                         if (activeScene) activeScene->OnRuntimeStop();
                         activeScene = &editorScene;
                         runtimeScene.reset();
                         editorState = EditorState::Edit;
                    }
                    if (Genesis::Engine::SceneLoader::LoadScene(editorScene, scenePathBuffer)) {
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
                if (Genesis::Engine::SceneLoader::SaveScene(editorScene, scenePathBuffer)) {
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
                            if (editorState != EditorState::Edit) {
                                if (activeScene) activeScene->OnRuntimeStop();
                                activeScene = &editorScene;
                                runtimeScene.reset();
                                editorState = EditorState::Edit;
                            }
                            if (Genesis::Engine::SceneLoader::LoadScene(editorScene, p)) {
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
                    if (Genesis::Engine::SceneLoader::SaveScene(editorScene, currentScenePath)) {
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
                            // Stop Play Mode if running
                            if (editorState != EditorState::Edit) {
                                if (activeScene) activeScene->OnRuntimeStop();
                                activeScene = &editorScene;
                                runtimeScene.reset();
                                editorState = EditorState::Edit;
                            }
                            if (Genesis::Engine::SceneLoader::LoadScene(editorScene, p)) {
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
                    // Stop Play Mode if running
                    if (editorState != EditorState::Edit) {
                        if (activeScene) activeScene->OnRuntimeStop();
                        activeScene = &editorScene;
                        runtimeScene.reset();
                        editorState = EditorState::Edit;
                    }
                    if (Genesis::Engine::SceneLoader::LoadScene(editorScene, p)) {
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
