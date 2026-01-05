#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/RendererManager.h"
#include "ENGINE/Texture.h"
#include "ENGINE/TextureRegistry.h"
#include <SDL.h>

TEST_CASE("TextureRegistry: handle registration and DestroyAllOnRenderer cleans up") {
    bool sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == true);

    SDL_Window* win = SDL_CreateWindow("TextureRegistryTest", 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"software"}, false);
    if (!r) r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
    REQUIRE(r != nullptr);
    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto cur = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(cur != nullptr);

    // If software isn't available, skip
    if (cur->GetName() != std::string("software")) {
        if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("Software renderer not available; skipping test");
        return;
    }

    std::vector<uint8_t> pixels = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
    auto tex = Genesis::Engine::Texture::CreateFromMemory(2,2,pixels);
    REQUIRE(tex != nullptr);

    Genesis::Engine::TextureRegistry::Instance().UploadAllToRenderer(cur);

    REQUIRE(tex->GetRendererHandle().IsValid());
    REQUIRE(tex->GetRendererOwner() == std::string("software"));

    // Now destroy all handles associated with this renderer
    Genesis::Engine::TextureRegistry::Instance().DestroyAllOnRenderer(cur);

    REQUIRE_FALSE(tex->GetRendererHandle().IsValid());
    REQUIRE(tex->GetRendererOwner().empty());

    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
