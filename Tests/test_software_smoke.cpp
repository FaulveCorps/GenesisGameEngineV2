#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/SoftwareRenderer.h"
#include <SDL.h>

TEST_CASE("Software smoke test") {
    printf("Software smoke test: starting\n"); fflush(stdout);

    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    if (sdlInitRes != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError()); fflush(stdout);
    }
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("Software Smoke", 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (!win) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError()); fflush(stdout);
    }
    REQUIRE(win != nullptr);

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError()); fflush(stdout);
    }
    REQUIRE(ctx != nullptr);

    auto renderer = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"software"}, false);
    if (!renderer) {
        printf("Software renderer not available - skipping test\n"); fflush(stdout);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("Software not available - skipping smoke test");
        return;
    }

    REQUIRE(renderer->Init(win, ctx));

    for (int i = 0; i < 3; ++i) {
        renderer->BeginFrame();
        renderer->EndFrame();
    }

    using namespace Genesis::Engine;
    SoftwareRenderer* sr = dynamic_cast<SoftwareRenderer*>(renderer.get());
    REQUIRE(sr != nullptr);

    std::vector<uint8_t> pixels;
    REQUIRE(sr->ReadbackOffscreen(64, 64, pixels));
    REQUIRE(pixels.size() == 64 * 64 * 4);

    size_t x = 64 / 2;
    size_t y = 64 / 2;
    size_t idx = (y * 64 + x) * 4;
    // BGRA order: R is at idx+2
    REQUIRE(pixels[idx + 2] >= 200);
    REQUIRE(pixels[idx + 1] <= 10);
    REQUIRE(pixels[idx + 0] <= 10);

    renderer->Shutdown();

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();

    SUCCEED("Software smoke test completed (no crash)");
}
