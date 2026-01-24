#include "catch_amalgamated.hpp"
#include "engine/GraphicsFactory.h"
#include "engine/RendererManager.h"
#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

static bool ShouldRunBenchmark() {
    const char* flag = std::getenv("GENESIS_ENABLE_BENCHMARK");
    return flag && std::strcmp(flag, "1") == 0;
}

TEST_CASE("Renderer micro-benchmark (opt-in)", "[benchmark][renderer]") {
    if (!ShouldRunBenchmark()) {
        SUCCEED("Benchmark disabled via GENESIS_ENABLE_BENCHMARK");
        return;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        FAIL("SDL_Init failed");
    }

    SDL_Window* win = SDL_CreateWindow("RendererBenchmark", 128, 128, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (!win) {
        SDL_Quit();
        FAIL("SDL_CreateWindow failed");
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        SDL_DestroyWindow(win);
        SDL_Quit();
        FAIL("SDL_GL_CreateContext failed");
    }

    auto renderer = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"opengl"}, false);
    if (!renderer) {
        renderer = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
    }
    REQUIRE(renderer != nullptr);

    Genesis::Engine::RendererManager::SetRenderer(std::move(renderer));
    auto current = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(current != nullptr);

    const int frames = 120;
    uint64_t start = SDL_GetPerformanceCounter();
    for (int i = 0; i < frames; ++i) {
        current->BeginFrame();
        current->EndFrame();
    }
    uint64_t end = SDL_GetPerformanceCounter();
    double elapsed = (double)(end - start) / (double)SDL_GetPerformanceFrequency();
    double avgMs = (elapsed / frames) * 1000.0;

    std::cout << "Renderer micro-benchmark (" << frames << " frames): avg " << avgMs << " ms/frame" << std::endl;
    REQUIRE(avgMs >= 0.0);

    current->Shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
