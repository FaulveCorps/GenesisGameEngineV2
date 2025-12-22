#include <iostream>
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/OpenGLRenderer.h"
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

    Genesis::Engine::OpenGLRenderer renderer;
    if (!renderer.Init(window.GetSDLWindow(), window.GetGLContext())) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return -1;
    }

    // Create scene and an entity with a model
    Genesis::Engine::Scene scene;

    auto entity = scene.Registry().create();
    auto modelPtr = std::make_shared<Genesis::Engine::Model>();
    if (!modelPtr->Load("assets/models/triangle.obj")) {
        std::cerr << "Failed to load model" << std::endl;
    }
    scene.Registry().emplace<Genesis::Engine::ModelComponent>(entity, Genesis::Engine::ModelComponent{ modelPtr });
    scene.Registry().emplace<Genesis::Engine::Transform>(entity, Genesis::Engine::Transform{});

    // Setup profiler and ImGui
    Genesis::Engine::Profiler profiler;
    Genesis::Engine::ImGuiLayer gui(window.GetSDLWindow(), window.GetGLContext());

    std::cout << "Entering main loop (close window to exit)..." << std::endl;

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

        // ImGui
        gui.NewFrame();
        gui.Render(profiler);

        renderer.EndFrame();
        profiler.EndFrame();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Unload plugins explicitly (optional)
    pluginManager.UnloadAll();

    renderer.Shutdown();
    window.Shutdown();
    Genesis::Engine::Shutdown();
    return 0;
}
