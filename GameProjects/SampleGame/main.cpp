#define SDL_MAIN_HANDLED
#include <iostream>
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/OpenGLRenderer.h"
#include "engine/DirectXRenderer.h"
#include "engine/VulkanRenderer.h"
#include "engine/Scene.h"

// Temporary: enable to skip loading GPU meshes and exercise DirectX backend only
// #define DIRECTX_SMOKE_TEST 0 // disabled to allow model loading for GL testing

// Quick smoke test for Vulkan renderer: define to try Vulkan path at startup
// #define VULKAN_SMOKE_TEST 1 (disabled for GL testing)
#include "engine/Components.h"
#include "engine/Model.h"
#include "engine/Profiler.h"
#include "engine/ImGuiLayer.h"
#include "engine/PluginManager.h"
#include "engine/Stats.h"
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    if (!Genesis::Engine::Init()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }

    Genesis::Engine::Window window;
    if (!window.Init("SampleGame - Genesis", 1280, 720)) {
        std::cerr << "Failed to create window" << std::endl;
        Genesis::Engine::Shutdown();
        return -1;
    }

    // Use OpenGLRenderer for main testing by default. Define VULKAN_SMOKE_TEST or DIRECTX_SMOKE_TEST to try other backends.
#ifdef VULKAN_SMOKE_TEST
    Genesis::Engine::VulkanRenderer renderer;
    if (!renderer.Init(window.GetSDLWindow(), window.GetGLContext())) {
        std::cerr << "Vulkan renderer initialization failed or Vulkan unavailable" << std::endl;
        // Continue running so we can see fallback behavior
    }
#elif defined(DIRECTX_SMOKE_TEST)
    Genesis::Engine::DirectXRenderer renderer;
    if (!renderer.Init(window.GetSDLWindow(), window.GetGLContext())) {
        std::cerr << "DirectX renderer initialization failed" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return -1;
    }
#else
    Genesis::Engine::OpenGLRenderer renderer;
    if (!renderer.Init(window.GetSDLWindow(), window.GetGLContext())) {
        std::cerr << "Failed to initialize OpenGL renderer" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return -1;
    }
#endif

    // Create scene and an entity with a model
    Genesis::Engine::Scene scene;

    auto entity = scene.Registry().create();
#ifdef DIRECTX_SMOKE_TEST
    std::cout << "DirectX smoke test: skipping model load" << std::endl;
#else
    auto modelPtr = std::make_shared<Genesis::Engine::Model>();
    if (!modelPtr->Load("assets/models/triangle.obj")) {
        std::cerr << "Failed to load model" << std::endl;
    } else {
        std::cout << "Model loaded successfully!" << std::endl;
        scene.Registry().emplace<Genesis::Engine::ModelComponent>(entity, Genesis::Engine::ModelComponent{ modelPtr });
        scene.Registry().emplace<Genesis::Engine::Transform>(entity, Genesis::Engine::Transform{});
    }
#endif

    // Setup profiler and ImGui
    Genesis::Engine::Profiler profiler;
    Genesis::Engine::ImGuiLayer gui(window.GetSDLWindow(), window.GetGLContext());

    std::cout << "Entering main loop (close window to exit)..." << std::endl;

#ifdef DIRECTX_SMOKE_TEST
    // Note: define DIRECTX_SMOKE_TEST in project CMake flags if running smoke test automatically
#endif

    // Attempt to load sample plugin (demonstrates plugin API)
    Genesis::Engine::PluginManager pluginManager;
    // Platform-specific extension
#ifdef _WIN32
    pluginManager.LoadPlugin("SamplePlugin.dll");
#else
    pluginManager.LoadPlugin("libSamplePlugin.so");
#endif

    while (window.PollEvents()) {
        profiler.BeginFrame();
        Genesis::Engine::Stats::Reset();

        renderer.BeginFrame();

        // scene update/render
        scene.Update(0.016);
        scene.Render();
        std::cout << "Main: after scene.Render" << std::endl;

    // Note: Model rendering now uses vertex arrays (faster than immediate mode)
        gui.Render(profiler);
        std::cout << "Main: after gui.Render" << std::endl;

        renderer.EndFrame();
        std::cout << "Main: after renderer.EndFrame" << std::endl;
        profiler.EndFrame();
        std::cout << "Main: after profiler.EndFrame" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Unload plugins explicitly (optional)
    pluginManager.UnloadAll();

    renderer.Shutdown();
    window.Shutdown();
    Genesis::Engine::Shutdown();
    return 0;
}
