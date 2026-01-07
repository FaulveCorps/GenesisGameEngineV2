#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>

#include "Engine/Engine.h"
#include "Engine/Window.h"
#include "Engine/OpenGLRenderer.h"
#include "Engine/Project.h"
#include "Engine/Scene.h"
#include "Engine/SceneLoader.h"
#include <SDL.h>

namespace fs = std::filesystem;

// Global callback workaround if needed, or just handle basic polling
void HandleEvents(const SDL_Event& event) {
    // Placeholder for global input handling if needed
}

int main(int argc, char* argv[]) {
    // 1. Parse Arguments
    std::string projectPathStr;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) {
            projectPathStr = argv[i + 1];
            i++;
        }
    }

    fs::path projectPath;
    if (projectPathStr.empty()) {
        // Check for Game.genesis in current dir
        if (fs::exists("Game.genesis")) {
            projectPath = fs::absolute("Game.genesis");
        } else {
            std::cerr << "Error: No project specified and Game.genesis not found." << std::endl;
            std::cerr << "Usage: GenesisRuntime --project <path/to/Game.genesis>" << std::endl;
            return 1;
        }
    } else {
        projectPath = fs::absolute(projectPathStr);
    }
    
    if (!fs::exists(projectPath)) {
        std::cerr << "Error: Project file not found: " << projectPath << std::endl;
        return 1;
    }

    // 2. Init Engine
    if (!Genesis::Engine::Init()) {
        std::cerr << "Failed to initialize Genesis Engine." << std::endl;
        return 1;
    }

    // 3. Load Project
    std::cout << "Loading project: " << projectPath << std::endl;
    auto project = Genesis::Engine::Project::Load(projectPath);
    if (!project) {
        std::cerr << "Failed to load project." << std::endl;
        Genesis::Engine::Shutdown();
        return 1;
    }
    Genesis::Engine::Project::SetActive(project);

    // Set CWD to project root (parent of Assets usually) to ensure relative paths in assets (e.g. "Assets/models/...") work
    fs::path newCwd = project->GetAssetDirectory().parent_path();
    fs::current_path(newCwd);
    std::cout << "Changed Working Directory to: " << newCwd << std::endl;

    // 4. Create Window
    Genesis::Engine::Window window;
    const auto& config = project->GetConfig();
    // Default resolution 1280x720, typical
    if (!window.Init(config.Name, 1280, 720)) {
         std::cerr << "Failed to create window." << std::endl;
         Genesis::Engine::Shutdown();
         return 1;
    }

    // 5. Create Renderer
    Genesis::Engine::OpenGLRenderer renderer;
    if (!renderer.Init(window.GetSDLWindow(), window.GetGLContext())) {
        std::cerr << "Failed to initialize renderer." << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return 1;
    }

    // 6. Load Scene
    Genesis::Engine::Scene scene;
    // Construct path ensuring we use the AssetDirectory correctly
    fs::path scenePath = project->GetAssetDirectory() / config.StartScene;
    
    std::cout << "Loading scene: " << scenePath << std::endl;
    if (!Genesis::Engine::SceneLoader::LoadScene(scene, scenePath.string())) {
        std::cerr << "Failed to load start scene: " << scenePath << std::endl;
        renderer.Shutdown();
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return 1;
    }

    // 7. Main Loop
    bool running = true;
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        double dt = deltaTime.count();

        // Poll Events
        if (!window.PollEvents(HandleEvents)) {
            running = false;
        }

        // Handle Resize (Basic)
        int w, h;
        SDL_GetWindowSize(window.GetSDLWindow(), &w, &h);
        scene.OnViewportResize(w, h); // User requested calling this
        
        // Update
        scene.Update(dt);

        // Render
        renderer.BeginFrame();
        scene.Render(&renderer);
        renderer.EndFrame();
        renderer.Present(); // Corresponds to Window::SwapBuffers
    }

    // Shutdown
    renderer.Shutdown();
    window.Shutdown();
    Genesis::Engine::Shutdown();

    return 0;
}
