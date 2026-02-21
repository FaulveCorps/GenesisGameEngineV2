#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include <SDL.h>

TEST_CASE("Vulkan renderer scene API smoke", "[renderer][vulkan][smoke]") {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SUCCEED("SDL video not available - skipping Vulkan scene API smoke");
        return;
    }

    SDL_Window* win = SDL_CreateWindow("VulkanSceneApiSmoke", 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (!win) {
        SDL_Quit();
        SUCCEED("SDL window creation failed - skipping Vulkan scene API smoke");
        return;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);

    auto renderer = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"vulkan"}, false);
    if (!renderer || renderer->GetName() != std::string("vulkan")) {
        if (ctx) SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("Vulkan renderer not available/present-capable in this environment - skipping smoke test");
        return;
    }

    float identity[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    renderer->SetViewProjection(identity, identity);

    float dir[3] = { 0.3f, 0.7f, 0.2f };
    float color[3] = { 1.0f, 0.9f, 0.8f };
    renderer->SetGlobalLight(dir, color, 1.2f);

    renderer->ClearPointLights();
    Genesis::Engine::IGraphicsAPI::PointLightData point{};
    point.position[0] = 0.0f; point.position[1] = 1.0f; point.position[2] = 2.0f;
    point.color[0] = 1.0f; point.color[1] = 0.1f; point.color[2] = 0.1f;
    point.intensity = 2.0f;
    point.radius = 5.0f;
    renderer->AddPointLight(point);

    renderer->SetPostProcessParams(1.1f, 2.2f);
    renderer->SetPostProcessBloom(true);
    renderer->SetPostProcessBloomThreshold(0.95f);
    renderer->SetPostProcessVignette(true, 0.25f, 0.7f, 0.2f);
    renderer->SetPostProcessLUT(nullptr, false, 1.0f);
    renderer->SetShadowParams(1.5f);

    Genesis::Engine::MeshDesc mesh;
    mesh.vertices = {
         0.0f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f
    };
    mesh.indices = { 0, 1, 2 };

    auto handle = renderer->CreateMesh(mesh);
    for (int i = 0; i < 2; ++i) {
        renderer->BeginFrame();
        if (handle.IsValid()) {
            renderer->DrawMesh(handle);
            renderer->DrawMesh(handle, nullptr, nullptr);
        }
        renderer->EndFrame();
    }

    if (handle.IsValid()) renderer->DestroyMesh(handle);

    renderer->Shutdown();
    if (ctx) SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();

    SUCCEED("Vulkan renderer accepted scene API smoke operations");
}
