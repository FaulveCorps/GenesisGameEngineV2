#define SDL_MAIN_HANDLED
#include <iostream>
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/OpenGLRenderer.h"
#include "engine/DirectXRenderer.h"
#include "engine/VulkanRenderer.h"
#include "engine/Scene.h"
#include "engine/GraphicsFactory.h"
#include "engine/RendererManager.h"
#include "engine/SoftwareRenderer.h"
#include <vector>

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
#include "engine/Shader.h"
#include "engine/ShaderRegistry.h"
#include "engine/TextureRegistry.h"
#include "engine/IPhysics.h"
#include "engine/IInput.h"
#include "engine/INetwork.h"
#include "engine/ISave.h"
#include <thread>
#include <chrono>
#include <filesystem>

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

    // Try to use SDL-backed input subsystem if available (falls back to null)
    if (!Genesis::Engine::CreateInputSubsystem("sdl")) {
        std::cout << "SampleGame: SDL input subsystem not available; using null input" << std::endl;
    } else {
        std::cout << "SampleGame: SDL input subsystem created" << std::endl;
    }

    // Use OpenGLRenderer for main testing by default. Define VULKAN_SMOKE_TEST or DIRECTX_SMOKE_TEST to try other backends.
// Renderer selection: prefer explicit smoke-test flags, otherwise try a configurable priority order via GraphicsFactory
#ifdef VULKAN_SMOKE_TEST
    // Explicit Vulkan smoke path
    {
        std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererInit = std::make_unique<Genesis::Engine::VulkanRenderer>();
        if (!rendererInit->Init(window.GetSDLWindow(), window.GetGLContext())) {
            std::cerr << "Vulkan smoke init failed or Vulkan unavailable" << std::endl;
            // continue so we can observe fallback behavior if desired
        }
        Genesis::Engine::RendererManager::SetRenderer(std::move(rendererInit));
    }
#elif defined(DIRECTX_SMOKE_TEST)
    // Explicit DirectX smoke path
    {
        std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererInit = std::make_unique<Genesis::Engine::DirectXRenderer>();
        if (!rendererInit->Init(window.GetSDLWindow(), window.GetGLContext())) {
            std::cerr << "DirectX smoke init failed" << std::endl;
            window.Shutdown();
            Genesis::Engine::Shutdown();
            return -1;
        }
        Genesis::Engine::RendererManager::SetRenderer(std::move(rendererInit));
    }
#else
    // Default: use runtime factory which tries Vulkan->DirectX->OpenGL (configurable via --gfx-order)
    std::vector<std::string> gfxOrder;
    bool gfxStrict = false;
    int autoCycleCount = 0;
    int autoCycleIntervalMs = 1000;
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
            // advance past the argument we just consumed
            ++i;
            continue;
        }
        if (std::string(argv[i]) == "--gfx-strict") {
            gfxStrict = true;
        }
        if (std::string(argv[i]) == "--auto-cycle" && i + 1 < argc) {
            try { autoCycleCount = std::stoi(argv[i+1]); } catch (...) { autoCycleCount = 0; }
            ++i;
            std::cout << "CLI: auto-cycle count set to " << autoCycleCount << std::endl;
            continue;
        }
        if (std::string(argv[i]) == "--auto-interval" && i + 1 < argc) {
            try { autoCycleIntervalMs = std::stoi(argv[i+1]); } catch (...) { autoCycleIntervalMs = 1000; }
            ++i;
            std::cout << "CLI: auto-cycle interval set to " << autoCycleIntervalMs << "ms" << std::endl;
            continue;
        }
        if (std::string(argv[i]) == "--vulkan-triangle") {
    #ifdef _WIN32
            _putenv_s("GENESIS_VULKAN_TRIANGLE", "1");
    #else
            setenv("GENESIS_VULKAN_TRIANGLE", "1", 1);
    #endif
            std::cout << "CLI: enabled GENESIS_VULKAN_TRIANGLE via --vulkan-triangle" << std::endl;
            continue;
        }
        if (std::string(argv[i]) == "--force-vulkan-swapchain") {
    #ifdef _WIN32
            _putenv_s("GENESIS_FORCE_VULKAN_SWAPCHAIN", "1");
    #else
            setenv("GENESIS_FORCE_VULKAN_SWAPCHAIN", "1", 1);
    #endif
            std::cout << "CLI: enabled GENESIS_FORCE_VULKAN_SWAPCHAIN via --force-vulkan-swapchain" << std::endl;
            continue;
        }
    }
    // Use factory (gfxStrict enforces swapchain/present capability for Vulkan)
    std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererInit = Genesis::Engine::GraphicsFactory::CreateRenderer(window.GetSDLWindow(), window.GetGLContext(), gfxOrder, gfxStrict);
    if (!rendererInit) {
        std::cerr << "Failed to initialize any renderer" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return -1;
    }

    // Adopt the renderer into the global manager so resources can be uploaded/destroyed centrally
    Genesis::Engine::RendererManager::SetRenderer(std::move(rendererInit));

    // If the selected renderer is the SoftwareRenderer, create a small secondary window and SDL renderer/texture
    // to present the CPU rasterized buffer so the user can visually confirm the software rendering.
    SDL_Window* softwareWindow = nullptr;
    SDL_Renderer* softwareSDLRenderer = nullptr;
    SDL_Texture* softwareTexture = nullptr;
    int softwareW = 640;
    int softwareH = 480;
    std::vector<uint8_t> softwarePixels;

    auto setupSoftwareVisual = [&](Genesis::Engine::IGraphicsAPI* r){
        // teardown existing
        if (softwareTexture) { SDL_DestroyTexture(softwareTexture); softwareTexture = nullptr; }
        if (softwareSDLRenderer) { SDL_DestroyRenderer(softwareSDLRenderer); softwareSDLRenderer = nullptr; }
        if (softwareWindow) { SDL_DestroyWindow(softwareWindow); softwareWindow = nullptr; }
        softwarePixels.clear();

        Genesis::Engine::SoftwareRenderer* sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(r);
        if (!sr) return;

        std::cout << "SampleGame: software renderer selected; creating visual output window" << std::endl;
        softwareWindow = SDL_CreateWindow("Software Output", SDL_WINDOWPOS_CENTERED + 40, SDL_WINDOWPOS_CENTERED + 40, softwareW, softwareH, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!softwareWindow) {
            std::cerr << "SampleGame: software visual window creation failed: " << SDL_GetError() << "; will save BMP to disk as fallback" << std::endl;
            return;
        }
        softwareSDLRenderer = SDL_CreateRenderer(softwareWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!softwareSDLRenderer) {
            std::cerr << "SampleGame: SDL_CreateRenderer failed for software output: " << SDL_GetError() << " - falling back to SDL_RENDERER_SOFTWARE\n";
            softwareSDLRenderer = SDL_CreateRenderer(softwareWindow, -1, SDL_RENDERER_SOFTWARE);
        }
        if (softwareSDLRenderer) {
            softwareTexture = SDL_CreateTexture(softwareSDLRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, softwareW, softwareH);
            if (!softwareTexture) {
                std::cerr << "SampleGame: SDL_CreateTexture failed for software output: " << SDL_GetError() << std::endl;
            } else {
                softwarePixels.resize(static_cast<size_t>(softwareW) * softwareH * 4);
            }
        }
    };

    // Initial setup based on the currently-selected renderer
    setupSoftwareVisual(Genesis::Engine::RendererManager::GetRenderer());

    // Update window title to include selected renderer
    if (auto cur = Genesis::Engine::RendererManager::GetRenderer()) {
        std::string title = std::string("SampleGame - Renderer: ") + cur->GetName();
        SDL_SetWindowTitle(window.GetSDLWindow(), title.c_str());
    }

    // Create small test shader to validate re-creation across renderer switches
    std::shared_ptr<Genesis::Engine::Shader> testShader;
    {
        const std::string testVert = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            void main() { gl_Position = vec4(aPos, 1.0); }
        )";
        const std::string testFrag = R"(
            #version 330 core
            out vec4 FragColor;
            void main() { FragColor = vec4(1.0); }
        )";
        testShader = Genesis::Engine::Shader::CreateFromSource(testVert, testFrag);
        if (testShader) {
            Genesis::Engine::ShaderRegistry::Instance().UploadAllToRenderer(Genesis::Engine::RendererManager::GetRenderer());
            std::cout << "SampleGame: created test shader id=" << testShader->GetID() << std::endl;
        }
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
    bool showHelp = false;
    bool do2dDemo = false;

    std::shared_ptr<Genesis::Engine::Texture> demoTex;

    bool doPhysicsDemo = false;
    bool doNetHost = false;
    int netHostPort = 0;
    std::string netConnectStr;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--stress") {
            stressMode = true;
            if (i + 1 < argc) {
                try { stressFrames = std::stoi(argv[i+1]); } catch (...) { stressFrames = -1; }
            } else {
                stressFrames = -1; // infinite
            }
            std::cout << "Stress mode enabled. frames=" << stressFrames << std::endl;
            break;
        }
        if (a == "--help" || a == "-h") {
            showHelp = true;
            break;
        }
        if (a == "--2d-demo") {
            do2dDemo = true;
            std::cout << "CLI: 2D demo mode enabled" << std::endl;
            continue;
        }
        if (a == "--physics-demo") {
            doPhysicsDemo = true;
            std::cout << "CLI: physics demo enabled" << std::endl;
            continue;
        }
        if (a == "--physics-2d") {
            doPhysicsDemo = true;
            std::cout << "CLI: 2D physics demo enabled" << std::endl;
            // user requested 2D demo
            // We'll attempt to create Box2D backend when starting the demo
            continue;
        }
        if (a == "--net-host" && i + 1 < argc) {
            try { netHostPort = std::stoi(argv[i+1]); doNetHost = true; } catch (...) { netHostPort = 0; }
            ++i;
            continue;
        }
        if (a == "--net-connect" && i + 1 < argc) {
            netConnectStr = argv[i+1];
            ++i;
            continue;
        }

    }

    if (showHelp) {
        std::cout << "SampleGame usage:\n";
        std::cout << "  --help|-h               Show this help message\n";
        std::cout << "  --stress [N]            Run in stress mode for N frames (omit N for infinite)\n";
        std::cout << "  --gfx-order A,B,C       Comma-separated renderer priority (examples: Vulkan,OpenGL or OpenGL,DirectX)\n";
        std::cout << "  --gfx-strict            Require Vulkan to be present-capable (swapchain + present) to be selected" << std::endl;
        std::cout << "  --vulkan-triangle       Opt-in: have Vulkan present a CPU-rasterized triangle (debug)" << std::endl;
        std::cout << "  --force-vulkan-swapchain Force swapchain creation even when SDL indicates no dynamic Vulkan support (risky)" << std::endl;
        std::cout << "  --net-host [port]       Host a small ENet server on the specified port" << std::endl;
        std::cout << "  --net-connect host:port Connect to a remote ENet server (host:port)" << std::endl;
        std::cout << "  F5                      Save to 'autosave' slot\n" << std::endl;
        std::cout << "  F6                      Load from 'autosave' slot\n" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return 0;
    }

    // 2D demo texture (in-memory) if requested
    if (do2dDemo) {
        std::vector<uint8_t> pixels = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255 };
        demoTex = Genesis::Engine::Texture::CreateFromMemory(2, 2, pixels);
        if (demoTex) Genesis::Engine::TextureRegistry::Instance().UploadAllToRenderer(Genesis::Engine::RendererManager::GetRenderer());
    }

#ifdef DIRECTX_SMOKE_TEST
    // Note: define DIRECTX_SMOKE_TEST in project CMake flags if running smoke test automatically
#endif

    // Attempt to load sample plugin (demonstrates plugin API)
    Genesis::Engine::PluginManager pluginManager;

    // If the user requested a physics demo, try to create a bullet physics subsystem and spawn a box
    Genesis::Engine::IPhysics::BodyHandle demoBody = 0;
    if (doPhysicsDemo) {
        // Prefer Box2D when 2D demo requested (or if it's available), otherwise try Bullet
        if (!Genesis::Engine::CreatePhysicsSubsystem("box2d")) {
            if (!Genesis::Engine::CreatePhysicsSubsystem("bullet")) {
                std::cerr << "SampleGame: No physics backend available; falling back to null physics demo" << std::endl;
                Genesis::Engine::CreatePhysicsSubsystem("null");
            } else {
                std::cout << "SampleGame: Bullet physics subsystem created" << std::endl;
            }
        } else {
            std::cout << "SampleGame: Box2D physics subsystem created" << std::endl;
        }
        auto ph = Genesis::Engine::GetPhysicsSubsystem();
        if (ph) {
            if (ph->Name() == "box2d") {
                auto a = ph->CreateBoxRigidBody(1.0f, -0.5f, 5.0f, 0.0f, 1.0f, 1.0f, 1.0f);
                auto b = ph->CreateBoxRigidBody(1.0f, 0.5f, 5.0f, 0.0f, 1.0f, 1.0f, 1.0f);
                if (a && b) {
                    auto j = ph->CreateDistanceJoint(a, b, -0.5f, 5.0f, 0.5f, 5.0f);
                    std::cout << "SampleGame: created 2D joint demo j=" << j << std::endl;
                }
                demoBody = a;
                if (demoBody != 0) std::cout << "SampleGame: created physics box demo handle=" << demoBody << std::endl;
            } else {
                // create a box at y=5 meters (3D Bullet or others)
                demoBody = ph->CreateBoxRigidBody(1.0f, 0.0f, 5.0f, 0.0f, 1.0f, 1.0f, 1.0f);
                if (demoBody != 0) std::cout << "SampleGame: created physics box demo handle=" << demoBody << std::endl;
            }
        }
    }
    // Platform-specific extension
#ifdef _WIN32
    pluginManager.LoadPlugin("SamplePlugin.dll");
#else
    pluginManager.LoadPlugin("libSamplePlugin.so");
#endif

    // Networking demo: attempt to create ENet backend and host/connect if requested
    if (doNetHost || !netConnectStr.empty()) {
        if (!Genesis::Engine::CreateNetworkSubsystem("enet")) {
            std::cout << "SampleGame: ENet backend not available; using null network" << std::endl;
            Genesis::Engine::CreateNetworkSubsystem("null");
        }
        auto net = Genesis::Engine::GetNetworkSubsystem();
        if (net) {
            if (doNetHost) {
                if (net->Host(static_cast<uint16_t>(netHostPort))) {
                    std::cout << "SampleGame: hosting on port " << netHostPort << std::endl;
                } else {
                    std::cout << "SampleGame: Host failed" << std::endl;
                }
            } else if (!netConnectStr.empty()) {
                size_t colon = netConnectStr.find(':');
                std::string host = netConnectStr;
                int port = 0;
                if (colon != std::string::npos) {
                    host = netConnectStr.substr(0, colon);
                    try { port = std::stoi(netConnectStr.substr(colon + 1)); } catch(...) { port = 0; }
                }
                if (net->Connect(host, static_cast<uint16_t>(port))) {
                    std::cout << "SampleGame: connecting to " << host << ":" << port << std::endl;
                } else {
                    std::cout << "SampleGame: connect failed" << std::endl;
                }
            }
        }
    }

    // Save subsystem: try file backend, fall back to null
    if (!Genesis::Engine::CreateSaveSubsystem("file")) {
        std::cout << "SampleGame: File save backend not available; using null save" << std::endl;
        Genesis::Engine::CreateSaveSubsystem("null");
    } else {
        std::cout << "SampleGame: File save subsystem created" << std::endl;
    }

    // Scripting: try Lua backend, fall back to null
    if (!Genesis::Engine::CreateScriptingSubsystem("lua")) {
        std::cout << "SampleGame: Lua scripting backend not available; using null scripting" << std::endl;
        Genesis::Engine::CreateScriptingSubsystem("null");
    } else {
        std::cout << "SampleGame: Lua scripting subsystem created" << std::endl;
    }

    // Mods: scan mods/ directory and display discovered mods
    {
        Genesis::Engine::ModManager mm;
        auto modsDir = std::filesystem::current_path() / "mods";
        if (mm.Scan(modsDir)) {
            auto mods = mm.Mods();
            std::cout << "SampleGame: discovered " << mods.size() << " mods" << std::endl;
            for (auto &m : mods) {
                std::cout << "  mod: id='" << m.id << "' name='" << m.name << "' version='" << m.version << "' path='" << m.path.string() << "'" << std::endl;
            }

            // If we have a scripting backend available, attempt to execute a 'mod.lua' in each mod folder
            auto sc = Genesis::Engine::GetScriptingSubsystem();
            if (sc && sc->Name() != "null") {
                for (auto &m : mods) {
                    auto script = m.path / "mod.lua";
                    if (std::filesystem::exists(script) && std::filesystem::is_regular_file(script)) {
                        if (sc->ExecuteFile(script.string())) std::cout << "Executed mod script: " << script.string() << std::endl;
                        else std::cout << "Failed to execute mod script: " << script.string() << std::endl;
                    }
                }
            }
        } else {
            std::cout << "SampleGame: no mods directory found" << std::endl;
        }
    }

    auto runFrame = [&](void){
        profiler.BeginFrame();
        // Update input subsystem once per frame after events are polled
        if (auto in = Genesis::Engine::GetInputSubsystem()) in->Update(1.0/60.0);
        // Poll networking subsystem if present
        if (auto net = Genesis::Engine::GetNetworkSubsystem()) net->Poll(1.0/60.0);
        Genesis::Engine::Stats::Reset();

        auto currentRenderer = Genesis::Engine::RendererManager::GetRenderer();
        if (currentRenderer) currentRenderer->BeginFrame();

        // scene update/render
        scene.Update(0.016);
        scene.Render();
        std::cout << "Main: after scene.Render" << std::endl;

        // Note: Model rendering now uses vertex arrays (faster than immediate mode)
        gui.Render(profiler);
        std::cout << "Main: after gui.Render" << std::endl;

        if (currentRenderer) currentRenderer->EndFrame();
        std::cout << "Main: after renderer.EndFrame" << std::endl;

        // Save/load hotkeys
        if (auto in = Genesis::Engine::GetInputSubsystem()) {
            if (in->WasKeyPressed(SDL_SCANCODE_F5)) {
                auto sv = Genesis::Engine::GetSaveSubsystem();
                if (sv) {
                    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    std::string data = "Saved at: " + std::to_string(now);
                    if (sv->Save("autosave", data)) std::cout << "Saved autosave slot" << std::endl;
                    else std::cout << "Save failed" << std::endl;
                }
            }
            if (in->WasKeyPressed(SDL_SCANCODE_F6)) {
                auto sv = Genesis::Engine::GetSaveSubsystem();
                if (sv) {
                    std::string data;
                    if (sv->Load("autosave", data)) std::cout << "Loaded autosave: " << data << std::endl;
                    else std::cout << "No autosave present" << std::endl;
                }
            }
        }

        // If the renderer is the software CPU renderer, read back the offscreen buffer each frame and present it
        if (auto sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(Genesis::Engine::RendererManager::GetRenderer())) {
            // Determine display size for software output (secondary window if present, otherwise main window size)
            if (softwareWindow) {
                int newW = softwareW, newH = softwareH;
                SDL_GetWindowSize(softwareWindow, &newW, &newH);
                if (newW <= 0) newW = 1;
                if (newH <= 0) newH = 1;
                if (newW != softwareW || newH != softwareH) {
                    softwareW = newW; softwareH = newH;
                    if (softwareTexture) { SDL_DestroyTexture(softwareTexture); softwareTexture = nullptr; }
                    if (softwareSDLRenderer) {
                        softwareTexture = SDL_CreateTexture(softwareSDLRenderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, softwareW, softwareH);
                        if (!softwareTexture) std::cerr << "SampleGame: SDL_CreateTexture failed on resize: " << SDL_GetError() << std::endl;
                    }
                    softwarePixels.clear();
                    softwarePixels.resize(static_cast<size_t>(softwareW) * softwareH * 4);
                }
            } else {
                int mainW = 0, mainH = 0;
                SDL_GetWindowSize(window.GetSDLWindow(), &mainW, &mainH);
                if (mainW != softwareW || mainH != softwareH) {
                    softwareW = mainW; softwareH = mainH;
                    softwarePixels.clear();
                    softwarePixels.resize(static_cast<size_t>(softwareW) * softwareH * 4);
                }
            }

            if (softwarePixels.empty()) softwarePixels.resize(static_cast<size_t>(softwareW) * softwareH * 4);

            // If 2D demo is enabled, queue a demo sprite
            if (do2dDemo && demoTex) {
                // draw centered sprite 1/4 of the window size
                float w = softwareW / 4.0f; float h = softwareH / 4.0f;
                float x = (softwareW - w) * 0.5f;
                float y = (softwareH - h) * 0.5f;
                Genesis::Engine::RendererManager::GetRenderer()->DrawTexture(demoTex.get(), x, y, w, h);
            }

            // If physics demo active, step simulation and occasionally print position
            if (doPhysicsDemo && demoBody != 0) {
                auto ph = Genesis::Engine::GetPhysicsSubsystem();
                if (ph) {
                    ph->StepSimulation(1.0f/60.0f, 1);
                    static int counter = 0; counter++;
                    if ((counter % 60) == 0) {
                        float px,py,pz; if (ph->GetRigidBodyPosition(demoBody, px,py,pz)) {
                            std::cout << "Physics demo: body pos=(" << px << "," << py << "," << pz << ")" << std::endl;
                        }
                    }
                }
            }

            if (sr->ReadbackOffscreen(static_cast<uint32_t>(softwareW), static_cast<uint32_t>(softwareH), softwarePixels)) {
                if (softwareTexture && softwareSDLRenderer) {
                    SDL_UpdateTexture(softwareTexture, nullptr, softwarePixels.data(), softwareW * 4);
                    SDL_RenderClear(softwareSDLRenderer);
                    SDL_RenderCopy(softwareSDLRenderer, softwareTexture, nullptr, nullptr);
                    SDL_RenderPresent(softwareSDLRenderer);
                } else {
                    static bool saved = false;
                    if (!saved) {
                        SDL_Surface* surf = SDL_CreateRGBSurfaceFrom((void*)softwarePixels.data(), softwareW, softwareH, 32, softwareW * 4,
                            0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
                        if (surf) {
                            std::string fname = "software_render.bmp";
                            if (SDL_SaveBMP(surf, fname.c_str()) == 0) {
                                std::cout << "SampleGame: saved software render output to " << fname << std::endl;
                                saved = true;
                            } else {
                                std::cerr << "SampleGame: failed to save BMP: " << SDL_GetError() << std::endl;
                            }
                            SDL_FreeSurface(surf);
                        } else {
                            std::cerr << "SampleGame: SDL_CreateRGBSurfaceFrom failed: " << SDL_GetError() << std::endl;
                        }
                    }
                }
            } else {
                std::cerr << "SampleGame: software ReadbackOffscreen failed" << std::endl;
            }
        }

        profiler.EndFrame();
        // Update window title with renderer name and FPS
        if (auto cur = Genesis::Engine::RendererManager::GetRenderer()) {
            char buf[128];
            snprintf(buf, sizeof(buf), "SampleGame - Renderer: %s | FPS: %.1f", cur->GetName().c_str(), profiler.GetFPS());
            SDL_SetWindowTitle(window.GetSDLWindow(), buf);
        }
        std::cout << "Main: after profiler.EndFrame" << std::endl;
    };

    Uint32 lastToggleTime = 0;
    // Auto-cycle support variables
    std::filesystem::path artifacts;
    int cyclesDone = 0;
    Uint32 lastAutoSwitchTime = SDL_GetTicks();
    if (autoCycleCount > 0) {
        artifacts = std::filesystem::current_path() / "artifacts";
        try {
            std::filesystem::create_directories(artifacts);
            std::cout << "SampleGame: artifacts dir created at " << artifacts.string() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "SampleGame: failed to create artifacts dir: " << e.what() << std::endl;
        }
    }

    if (stressMode) {
        if (stressFrames == -1) std::cout << "Stress: running until closed or crash" << std::endl;
        int frames = 0;
        while ((stressFrames == -1 || frames < stressFrames) && window.PollEvents()) {
            runFrame();
            ++frames;
            // Allow runtime renderer cycling on F2 (debounced)
            {
                auto in = Genesis::Engine::GetInputSubsystem();
                Uint32 now = SDL_GetTicks();
                if (in && (in->WasKeyPressed(SDL_SCANCODE_F2) || in->WasControllerButtonPressed(0, SDL_CONTROLLER_BUTTON_START)) && now - lastToggleTime > 300) {
                    lastToggleTime = now;
                    if (Genesis::Engine::RendererManager::CycleRenderer(window.GetSDLWindow(), window.GetGLContext())) {
                        setupSoftwareVisual(Genesis::Engine::RendererManager::GetRenderer());
                        std::cout << "SampleGame: cycled renderer (stress)" << std::endl;
                    }
                }
            }

            // Auto-cycle handling (stress mode)
            if (autoCycleCount > 0) {
                Uint32 nowAuto = SDL_GetTicks();
                if (nowAuto - lastAutoSwitchTime >= static_cast<Uint32>(autoCycleIntervalMs)) {
                    lastAutoSwitchTime = nowAuto;
                    if (Genesis::Engine::RendererManager::CycleRenderer(window.GetSDLWindow(), window.GetGLContext())) {
                        setupSoftwareVisual(Genesis::Engine::RendererManager::GetRenderer());
                        Genesis::Engine::ShaderRegistry::Instance().UploadAllToRenderer(Genesis::Engine::RendererManager::GetRenderer());
                        std::cout << "SampleGame: auto-cycled renderer (stress) cyclesDone=" << cyclesDone << std::endl;
                        // If software renderer, save screenshot
                        if (auto sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(Genesis::Engine::RendererManager::GetRenderer())) {
                            std::vector<uint8_t> pixels;
                            if (sr->ReadbackOffscreen(static_cast<uint32_t>(softwareW), static_cast<uint32_t>(softwareH), pixels)) {
                                SDL_Surface* surf = SDL_CreateRGBSurfaceFrom((void*)pixels.data(), softwareW, softwareH, 32, softwareW * 4,
                                    0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
                                if (surf) {
                                    std::string fname = (artifacts / ("screenshot_auto_stress_" + std::to_string(cyclesDone) + ".bmp")).string();
                                    if (SDL_SaveBMP(surf, fname.c_str()) == 0) {
                                        std::cout << "SampleGame: saved auto screenshot to " << fname << std::endl;
                                    } else {
                                        std::cerr << "SampleGame: failed to save auto BMP: " << SDL_GetError() << std::endl;
                                    }
                                    SDL_FreeSurface(surf);
                                }
                            } else {
                                std::cerr << "SampleGame: auto ReadbackOffscreen failed (stress)" << std::endl;
                            }
                        }
                        ++cyclesDone;
                        if (cyclesDone >= autoCycleCount) {
                            std::cout << "SampleGame: auto-cycle complete (" << cyclesDone << " cycles). Exiting." << std::endl;
                            break;
                        }
                    }
                }
            }

            // No sleeping in stress mode to increase chance of reproducing intermittent bugs
            if ((frames % 1000) == 0) std::cout << "Stress: completed frames=" << frames << std::endl;
        }
    } else {
        while (window.PollEvents()) {
            runFrame();

            // Allow runtime renderer cycling on F2 (debounced)
            {
                auto in = Genesis::Engine::GetInputSubsystem();
                Uint32 now = SDL_GetTicks();
                if (in && (in->WasKeyPressed(SDL_SCANCODE_F2) || in->WasControllerButtonPressed(0, SDL_CONTROLLER_BUTTON_START)) && now - lastToggleTime > 300) {
                    lastToggleTime = now;
                    if (Genesis::Engine::RendererManager::CycleRenderer(window.GetSDLWindow(), window.GetGLContext())) {
                        setupSoftwareVisual(Genesis::Engine::RendererManager::GetRenderer());
                        std::cout << "SampleGame: cycled renderer" << std::endl;
                    }
                }
            }

            // Auto-cycle handling (interactive mode)
            if (autoCycleCount > 0) {
                Uint32 nowAuto = SDL_GetTicks();
                if (nowAuto - lastAutoSwitchTime >= static_cast<Uint32>(autoCycleIntervalMs)) {
                    lastAutoSwitchTime = nowAuto;
                    if (Genesis::Engine::RendererManager::CycleRenderer(window.GetSDLWindow(), window.GetGLContext())) {
                        setupSoftwareVisual(Genesis::Engine::RendererManager::GetRenderer());
                        Genesis::Engine::ShaderRegistry::Instance().UploadAllToRenderer(Genesis::Engine::RendererManager::GetRenderer());
                        if (testShader) std::cout << "SampleGame: testShader->GetID()=" << testShader->GetID() << std::endl;
                        std::cout << "SampleGame: auto-cycled renderer cyclesDone=" << cyclesDone << std::endl;
                        if (auto sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(Genesis::Engine::RendererManager::GetRenderer())) {
                            std::vector<uint8_t> pixels;
                            if (sr->ReadbackOffscreen(static_cast<uint32_t>(softwareW), static_cast<uint32_t>(softwareH), pixels)) {
                                SDL_Surface* surf = SDL_CreateRGBSurfaceFrom((void*)pixels.data(), softwareW, softwareH, 32, softwareW * 4,
                                    0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
                                if (surf) {
                                    std::string fname = (artifacts / ("screenshot_auto_" + std::to_string(cyclesDone) + ".bmp")).string();
                                    if (SDL_SaveBMP(surf, fname.c_str()) == 0) {
                                        std::cout << "SampleGame: saved auto screenshot to " << fname << std::endl;
                                    } else {
                                        std::cerr << "SampleGame: failed to save auto BMP: " << SDL_GetError() << std::endl;
                                    }
                                    SDL_FreeSurface(surf);
                                }
                            } else {
                                std::cerr << "SampleGame: auto ReadbackOffscreen failed" << std::endl;
                            }
                        }
                        ++cyclesDone;
                        if (cyclesDone >= autoCycleCount) {
                            std::cout << "SampleGame: auto-cycle complete (" << cyclesDone << " cycles). Exiting." << std::endl;
                            break;
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    // Unload plugins explicitly (optional)
    pluginManager.UnloadAll();

    // Tear down secondary software visual resources if present
    if (softwareTexture) { SDL_DestroyTexture(softwareTexture); softwareTexture = nullptr; }
    if (softwareSDLRenderer) { SDL_DestroyRenderer(softwareSDLRenderer); softwareSDLRenderer = nullptr; }
    if (softwareWindow) { SDL_DestroyWindow(softwareWindow); softwareWindow = nullptr; }

    if (auto cur = Genesis::Engine::RendererManager::GetRenderer()) cur->Shutdown();
    window.Shutdown();
    Genesis::Engine::Shutdown();
    return 0;
}
