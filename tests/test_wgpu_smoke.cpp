#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#ifdef HAVE_WGPU
#include "ENGINE/WgpuRenderer.h"
#endif
#include <SDL.h>
#include <cstdlib>
#include <cstring>

TEST_CASE("WGPU smoke test") {
    printf("WGPU smoke test: starting\n"); fflush(stdout);
    // Also write a small trace file so we can detect whether the test reached this point
    {
        FILE* f = fopen("C:\\Users\\jpfau\\Desktop\\Project\\GenesisGameEngine\\wgpu_test_trace.txt", "w");
        if (f) {
            fprintf(f, "test started\n");
            fclose(f);
        }
    }

    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    if (sdlInitRes != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError()); fflush(stdout);
    }
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("WGPU Smoke", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (!win) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError()); fflush(stdout);
    }
    REQUIRE(win != nullptr);

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError()); fflush(stdout);
    }

    auto renderer = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"wgpu"}, false);
    if (!renderer) {
        // If wgpu is not available on the system running the tests, that's acceptable - just skip.
        printf("Wgpu renderer not available - skipping test\n"); fflush(stdout);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("Wgpu not available - skipping smoke test");
        return;
    }

    // Only run the WGPU smoke test if explicitly enabled via env var
    {
        const char* enable = std::getenv("GENESIS_ENABLE_WGPU_TESTS");
        if (!enable || std::strcmp(enable, "1") != 0) {
            printf("Wgpu smoke test disabled by GENESIS_ENABLE_WGPU_TESTS; skipping\n"); fflush(stdout);
            renderer->Shutdown();
            SDL_GL_DeleteContext(ctx);
            SDL_DestroyWindow(win);
            SDL_Quit();
            SUCCEED("Wgpu not enabled in environment - skipping smoke test");
            return;
        }
    }

    REQUIRE(renderer->Init(win, ctx));

    // Run a few frames to exercise swapchain acquire/present and device tick
    for (int i = 0; i < 3; ++i) {
        renderer->BeginFrame();
        renderer->EndFrame();
    }

#ifdef HAVE_WGPU
    // Attempt an offscreen render and readback to verify a triangle (red) was rendered
    {
        #include "ENGINE/WgpuRenderer.h"
        using namespace Genesis::Engine;
        WgpuRenderer* wr = dynamic_cast<WgpuRenderer*>(renderer.get());
        if (wr) {
            std::vector<uint8_t> pixels;
            REQUIRE(wr->ReadbackOffscreen(64, 64, pixels));
            REQUIRE(pixels.size() == 64 * 64 * 4);
            size_t x = 64/2;
            size_t y = 64/2;
            size_t idx = (y * 64 + x) * 4;
            // BGRA order: R is at idx+2
            REQUIRE(pixels[idx + 2] >= 200);
            REQUIRE(pixels[idx + 1] <= 10);
            REQUIRE(pixels[idx + 0] <= 10);
        }
    }
#endif

    renderer->Shutdown();

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();

    SUCCEED("Wgpu smoke test completed (no crash)");
}
