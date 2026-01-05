#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/RendererManager.h"
#include "ENGINE/Shader.h"
#include "ENGINE/ShaderRegistry.h"
#include <SDL.h>
#include <cstdlib>
#include <cstring>

TEST_CASE("Shader re-creation across renderer switches") {
    bool sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == true);

    SDL_Window* win = SDL_CreateWindow("ShaderRecreateTest", 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    // Optionally allow explicitly running OpenGL tests via env var
    const char* _enableGL = std::getenv("GENESIS_ENABLE_OPENGL");
    if (!_enableGL || std::strcmp(_enableGL, "1") != 0) {
        // Skip tests that explicitly require OpenGL if not enabled
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("OpenGL tests disabled via GENESIS_ENABLE_OPENGL");
        return;
    }

    // Start with OpenGL renderer if available
    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, { "opengl" }, false);
    if (!r) {
        // Fallback: pick any renderer
        r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
        REQUIRE(r != nullptr);
    }

    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto cur = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(cur != nullptr);

    // Create a simple shader and register it with the registry
    const std::string vert = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        void main() { gl_Position = vec4(aPos, 1.0); }
    )";
    const std::string frag = R"(
        #version 330 core
        out vec4 FragColor;
        void main() { FragColor = vec4(1.0); }
    )";

    auto sh = Genesis::Engine::Shader::CreateFromSource(vert, frag);
    REQUIRE(sh != nullptr);

    // Ask registry to upload to current renderer
    Genesis::Engine::ShaderRegistry::Instance().UploadAllToRenderer(cur);
    // If current renderer is OpenGL and GL is available, GetID should be non-zero; otherwise may remain 0
    unsigned int idBefore = sh->GetID();

    // Switch to software renderer
    bool switched = Genesis::Engine::RendererManager::SwitchRendererByName("software", win, ctx);
    // If switch didn't work, that's okay (no software candidate); otherwise ensure shader got destroyed
    if (switched) {
        REQUIRE(Genesis::Engine::RendererManager::GetRenderer() != nullptr);
        REQUIRE(sh->GetID() == 0);

        // Switch back to OpenGL
        bool back = Genesis::Engine::RendererManager::SwitchRendererByName("opengl", win, ctx);
        if (back) {
            REQUIRE(Genesis::Engine::RendererManager::GetRenderer() != nullptr);
            // After switching back, expect shader to be re-uploaded
            Genesis::Engine::ShaderRegistry::Instance().UploadAllToRenderer(Genesis::Engine::RendererManager::GetRenderer());
            REQUIRE(sh->GetID() != 0);
        }
    }

    // Cleanup
    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
