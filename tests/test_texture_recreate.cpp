#include "catch_amalgamated.hpp"
#include "engine/GraphicsFactory.h"
#include "engine/RendererManager.h"
#include "engine/Texture.h"
#include "engine/TextureRegistry.h"
#include <SDL.h>

TEST_CASE("Texture re-creation across renderer switches") {
    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("TextureRecreateTest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    // Try to initialize OpenGL renderer first
    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"opengl"}, false);
    if (!r) {
        // Fallback: pick any renderer
        r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
        REQUIRE(r != nullptr);
    }

    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto cur = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(cur != nullptr);

    // Create small 2x2 RGBA texture
    std::vector<uint8_t> pixels = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255 };
    auto tex = Genesis::Engine::Texture::CreateFromMemory(2, 2, pixels);
    REQUIRE(tex != nullptr);

    // Upload to current renderer
    Genesis::Engine::TextureRegistry::Instance().UploadAllToRenderer(cur);
    unsigned int idBefore = tex->GetID();

    // Switch to software renderer
    bool switched = Genesis::Engine::RendererManager::SwitchRendererByName("software", win, ctx);
    if (switched) {
        REQUIRE(Genesis::Engine::RendererManager::GetRenderer() != nullptr);
        REQUIRE(tex->GetID() == 0);

        // Switch back to OpenGL
        bool back = Genesis::Engine::RendererManager::SwitchRendererByName("opengl", win, ctx);
        if (back) {
            REQUIRE(Genesis::Engine::RendererManager::GetRenderer() != nullptr);
            Genesis::Engine::TextureRegistry::Instance().UploadAllToRenderer(Genesis::Engine::RendererManager::GetRenderer());
            REQUIRE(tex->GetID() != 0);
        }
    }

    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
