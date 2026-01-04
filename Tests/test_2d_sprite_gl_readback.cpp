#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/RendererManager.h"
#include "ENGINE/Texture.h"
#include "ENGINE/OpenGLRenderer.h"
#include <SDL.h>

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

TEST_CASE("2D sprite readback (opengl)") {
    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("2DGLReadback", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"opengl"}, false);
    if (!r) {
        // OpenGL may not be available on some runners; treat as skipped
        SUCCEED("OpenGL renderer not available - skipping GL readback test");
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return;
    }
    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto gr = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(gr != nullptr);

    // Disable swap for readback so we can read the back buffer
    if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(gr)) {
        glRenderer->SetPresentEnabled(false);
    }

    // Create a small 2x2 RGBA texture
    std::vector<uint8_t> pixels = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255 };
    auto tex = Genesis::Engine::Texture::CreateFromMemory(2, 2, pixels);
    REQUIRE(tex != nullptr);

    // Draw a scaled sprite to the center of a 64x64 target
    float sx = 24.0f, sy = 24.0f, sw = 16.0f, sh = 16.0f;

    gr->BeginFrame();
    gr->DrawTexture(tex.get(), sx, sy, sw, sh);
    gr->EndFrame();

    // Ensure GL context current
    SDL_GL_MakeCurrent(win, ctx);

    // Resolve glReadPixels
    auto addrReadPixels = (void*)SDL_GL_GetProcAddress("glReadPixels");
    if (!addrReadPixels) {
        SUCCEED("glReadPixels not available - skipping GL readback test");
        if (auto cur = Genesis::Engine::RendererManager::GetRenderer()) cur->Shutdown();
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return;
    }

    using PFNGLREADPIXELSPROC = void (APIENTRY*)(int, int, int, int, unsigned int, unsigned int, void*);
    PFNGLREADPIXELSPROC pglReadPixels = (PFNGLREADPIXELSPROC)addrReadPixels;

    int cx = static_cast<int>(sx + sw / 2.0f);
    int cy = static_cast<int>(sy + sh / 2.0f);
    int w = 64, h = 64;
    int readY = (h - 1) - cy; // convert to GL's lower-left origin

    unsigned char pix[4] = {0,0,0,0};
    pglReadPixels(cx, readY, 1, 1, 0x1908 /*GL_RGBA*/, 0x1401 /*GL_UNSIGNED_BYTE*/, pix);

    // Center pixel should not be black
    REQUIRE((pix[0] >= 10 || pix[1] >= 10 || pix[2] >= 10));

    if (auto cur = Genesis::Engine::RendererManager::GetRenderer()) cur->Shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
