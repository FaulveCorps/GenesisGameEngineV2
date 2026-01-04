#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/RendererManager.h"
#include "ENGINE/Texture.h"
#include "ENGINE/TextureRegistry.h"
#include "ENGINE/SoftwareRenderer.h"
#include <SDL.h>

TEST_CASE("2D sprite smoke (software)") {
    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("2DTest", 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"software"}, false);
    if (!r) {
        r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
        REQUIRE(r != nullptr);
    }
    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto cur = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(cur != nullptr);

    // Create a small 2x2 RGBA texture (center color test)
    std::vector<uint8_t> pixels = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255 };
    auto tex = Genesis::Engine::Texture::CreateFromMemory(2, 2, pixels);
    REQUIRE(tex != nullptr);

    // Draw a scaled sprite to the center of a 64x64 target
    float sx = 24.0f, sy = 24.0f, sw = 16.0f, sh = 16.0f;

    auto gr = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(gr != nullptr);

    gr->BeginFrame();
    gr->DrawTexture(tex.get(), sx, sy, sw, sh);
    gr->EndFrame();

    auto sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(gr);
    REQUIRE(sr != nullptr);

    std::vector<uint8_t> out;
    REQUIRE(sr->ReadbackOffscreen(64, 64, out));
    size_t cx = static_cast<size_t>(sx + sw/2);
    size_t cy = static_cast<size_t>(sy + sh/2);
    size_t idx = (cy * 64 + cx) * 4;

    // Center pixel should not be black
    REQUIRE((out[idx + 2] >= 10 || out[idx + 1] >= 10 || out[idx + 0] >= 10));

    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
