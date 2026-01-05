#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/RendererManager.h"
#include "ENGINE/Texture.h"
#include <SDL.h>

TEST_CASE("2D sprite smoke (opengl)") {
    bool sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == true);

    SDL_Window* win = SDL_CreateWindow("2DTestGL", 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"opengl"}, false);
    if (!r) {
        // OpenGL may not be available on some CI runners; treat as skipped
        SUCCEED("OpenGL renderer not available - skipping GL smoke test");
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return;
    }

    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto gr = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(gr != nullptr);

    // Create a small 2x2 RGBA texture
    std::vector<uint8_t> pixels = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255 };
    auto tex = Genesis::Engine::Texture::CreateFromMemory(2, 2, pixels);
    REQUIRE(tex != nullptr);

    gr->BeginFrame();
    gr->DrawTexture(tex.get(), 24.0f, 24.0f, 16.0f, 16.0f);
    gr->EndFrame();

    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
