#define SDL_MAIN_HANDLED
#include <iostream>
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/OpenGLRenderer.h"
#include "engine/DirectXRenderer.h"
#include "engine/VulkanRenderer.h"
#include "engine/Scene.h"
#include "engine/GraphicsFactory.h"

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
// Renderer selection: prefer explicit smoke-test flags, otherwise try a configurable priority order via GraphicsFactory
#ifdef VULKAN_SMOKE_TEST
    // Explicit Vulkan smoke path
    std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererPtr = std::make_unique<Genesis::Engine::VulkanRenderer>();
    if (!rendererPtr->Init(window.GetSDLWindow(), window.GetGLContext())) {
        std::cerr << "Vulkan smoke init failed or Vulkan unavailable" << std::endl;
        // continue so we can observe fallback behavior if desired
    }
#elif defined(DIRECTX_SMOKE_TEST)
    // Explicit DirectX smoke path
    std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererPtr = std::make_unique<Genesis::Engine::DirectXRenderer>();
    if (!rendererPtr->Init(window.GetSDLWindow(), window.GetGLContext())) {
        std::cerr << "DirectX smoke init failed" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return -1;
    }
#else
    // Default: use runtime factory which tries Vulkan->DirectX->OpenGL (configurable via --gfx-order)
    std::vector<std::string> gfxOrder;
    bool gfxStrict = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--gfx-order" && i + 1 < argc) {
            std::string arg = argv[i+1];
            // split by comma
            size_t start = 0;
            while (start < arg.size()) {
                size_t comma = arg.find(',', start);
                if (comma == std::string::npos) comma = arg.size();
                gfxOrder.push_back(arg.substr(start, comma - start));
                start = comma + 1;
            }
            break;
        }
        if (std::string(argv[i]) == "--gfx-strict") {
            gfxStrict = true;
        }
    }
    // Use factory (gfxStrict enforces swapchain/present capability for Vulkan)
    std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererPtr = Genesis::Engine::GraphicsFactory::CreateRenderer(window.GetSDLWindow(), window.GetGLContext(), gfxOrder, gfxStrict);
    if (!rendererPtr) {
        std::cerr << "Failed to initialize any renderer" << std::endl;
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

    bool stressMode = false;
    int stressFrames = 0; // 0 == disabled, -1 == infinite
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--stress") {
            stressMode = true;
            if (i + 1 < argc) {
                try { stressFrames = std::stoi(argv[i+1]); } catch (...) { stressFrames = -1; }
            } else {
                stressFrames = -1; // infinite
            }
            std::cout << "Stress mode enabled. frames=" << stressFrames << std::endl;
            break;
        }
    }

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

    auto runFrame = [&](void){
        profiler.BeginFrame();
        Genesis::Engine::Stats::Reset();

        rendererPtr->BeginFrame();

        // scene update/render
        scene.Update(0.016);
        scene.Render();
        std::cout << "Main: after scene.Render" << std::endl;

        // Note: Model rendering now uses vertex arrays (faster than immediate mode)
        gui.Render(profiler);
        std::cout << "Main: after gui.Render" << std::endl;

        rendererPtr->EndFrame();
        std::cout << "Main: after renderer.EndFrame" << std::endl;
        profiler.EndFrame();
        std::cout << "Main: after profiler.EndFrame" << std::endl;
    };

    if (stressMode) {
        if (stressFrames == -1) std::cout << "Stress: running until closed or crash" << std::endl;
        int frames = 0;
        while ((stressFrames == -1 || frames < stressFrames) && window.PollEvents()) {
            runFrame();
            ++frames;
            // No sleeping in stress mode to increase chance of reproducing intermittent bugs
            if ((frames % 1000) == 0) std::cout << "Stress: completed frames=" << frames << std::endl;
        }
    } else {
        while (window.PollEvents()) {
            runFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    // Unload plugins explicitly (optional)
    pluginManager.UnloadAll();

    rendererPtr->Shutdown();
    window.Shutdown();
    Genesis::Engine::Shutdown();
    return 0;
}
