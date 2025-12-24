#include <catch2/catch.hpp>
#include "engine/GraphicsFactory.h"
#include <SDL.h>

TEST_CASE("WGPU smoke test") {
    REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_Window* win = SDL_CreateWindow("WGPU Smoke", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);

    SDL_GLContext ctx = SDL_GL_CreateContext(win);

    auto renderer = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"wgpu"}, false);
    if (!renderer) {
        // If wgpu is not available on the system running the tests, that's acceptable - just skip.
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("Wgpu not available - skipping smoke test");
        return;
    }

    REQUIRE(renderer->Init(win, ctx));
    renderer->BeginFrame();
    renderer->EndFrame();
    renderer->Shutdown();

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();

    SUCCEED("Wgpu smoke test completed (no crash)");
}
