#include "catch_amalgamated.hpp"
#include "engine/GraphicsFactory.h"
#include "engine/RendererManager.h"
#include "engine/SoftwareRenderer.h"
#include "engine/Mesh.h"
#include <SDL.h>

TEST_CASE("Renderer runtime switch basic") {
    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("RendererSwitchTest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    // Start with the factory default (or software if forced)
    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"software"}, false);
    if (!r) {
        // Fallback: try any renderer - CreateRenderer will pick the first available
        r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
        REQUIRE(r != nullptr);
    }

    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto cur = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(cur != nullptr);

    // Create a simple mesh and model to register resources
    Genesis::Engine::Mesh m;
    std::vector<float> verts = { 0.0f,0.5f,0.0f,  -0.5f,-0.5f,0.0f,  0.5f,-0.5f,0.0f };
    std::vector<float> norms; // empty
    std::vector<uint32_t> idx = {0,1,2};
    m.SetData(verts, norms, idx);

    // Upload to current renderer
    m.UploadToGPU();

    // Try cycling a few times and perform a frame on each backend
    bool anySwitched = false;
    for (int i = 0; i < 5; ++i) {
        bool ok = Genesis::Engine::RendererManager::CycleRenderer(win, ctx);
        if (!ok) continue; // maybe candidate not available
        anySwitched = true;
        auto cur2 = Genesis::Engine::RendererManager::GetRenderer();
        REQUIRE(cur2 != nullptr);
        // simulate a small frame
        cur2->BeginFrame();
        // Draw: call mesh draw which will dispatch to renderer
        m.Draw();
        cur2->EndFrame();

        // If software renderer, ensure ReadbackOffscreen works
        if (auto sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(cur2)) {
            std::vector<uint8_t> pixels;
            REQUIRE(sr->ReadbackOffscreen(64, 64, pixels));
            REQUIRE(pixels.size() == 64 * 64 * 4);
        }
    }

    REQUIRE(anySwitched == true);

    // Cleanup
    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}