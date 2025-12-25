#include "catch_amalgamated.hpp"
#include "engine/GraphicsFactory.h"
#include "engine/RendererManager.h"
#include "ENGINE/Texture.h"
#include "ENGINE/TextureRegistry.h"
#include <SDL.h>
#include <string>

TEST_CASE("Texture file load and recreate") {
    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("TextureFileTest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    // Create a small surf, save BMP to disk, then load via Texture::CreateFromFile
    const int w = 2, h = 2;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(nullptr, w, h, 32, w*4, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surf != nullptr);
    // Fill pixels: RGBA
    uint8_t data[w*h*4] = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255 };
    memcpy(surf->pixels, data, sizeof(data));
    std::string fname = "temp_test_tex.bmp";
    REQUIRE(SDL_SaveBMP(surf, fname.c_str()) == 0);
    SDL_FreeSurface(surf);

    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"opengl"}, false);
    if (!r) r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
    REQUIRE(r != nullptr);
    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto cur = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(cur != nullptr);

    auto tex = Genesis::Engine::Texture::CreateFromFile(fname);
    REQUIRE(tex != nullptr);
    Genesis::Engine::TextureRegistry::Instance().UploadAllToRenderer(cur);
    REQUIRE(tex->GetID() != 0);

    // Cleanup
    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();

    // Remove test file
    remove(fname.c_str());
}
