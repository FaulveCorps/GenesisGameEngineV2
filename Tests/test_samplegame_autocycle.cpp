#include "catch_amalgamated.hpp"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/RendererManager.h"
#include "ENGINE/SoftwareRenderer.h"
#include "ENGINE/Shader.h"
#include "ENGINE/ShaderRegistry.h"
#include <SDL.h>
#include <filesystem>
#include <iostream>

TEST_CASE("SampleGame auto-cycle smoke test") {
    int sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == 0);

    SDL_Window* win = SDL_CreateWindow("AutoCycleTest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    auto r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"software"}, false);
    if (!r) {
        r = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {}, false);
        REQUIRE(r != nullptr);
    }

    Genesis::Engine::RendererManager::SetRenderer(std::move(r));
    auto cur = Genesis::Engine::RendererManager::GetRenderer();
    REQUIRE(cur != nullptr);

    // Create a small test shader to participate in registry uploads
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
    Genesis::Engine::ShaderRegistry::Instance().UploadAllToRenderer(cur);
    unsigned int idBefore = sh->GetID();

    int cycles = 5;
    bool anySwitch = false;
    for (int i = 0; i < cycles; ++i) {
        bool ok = Genesis::Engine::RendererManager::CycleRenderer(win, ctx);
        if (!ok) continue;
        anySwitch = true;
        auto cur2 = Genesis::Engine::RendererManager::GetRenderer();
        REQUIRE(cur2 != nullptr);
        Genesis::Engine::ShaderRegistry::Instance().UploadAllToRenderer(cur2);
        std::cout << "Cycle " << i << ": testShader id=" << sh->GetID() << std::endl;

        if (auto sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(cur2)) {
            std::vector<uint8_t> pixels;
            REQUIRE(sr->ReadbackOffscreen(64, 64, pixels));
            REQUIRE(pixels.size() == 64 * 64 * 4);
            // Save artifact
            std::filesystem::path artifacts = std::filesystem::current_path() / "artifacts";
            try { std::filesystem::create_directories(artifacts); } catch(...) {}
            SDL_Surface* surf = SDL_CreateRGBSurfaceFrom((void*)pixels.data(), 64, 64, 32, 64*4,
                0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
            if (surf) {
                std::string fname = (artifacts / ("test_cycle_" + std::to_string(i) + ".bmp")).string();
                SDL_SaveBMP(surf, fname.c_str());
                SDL_FreeSurface(surf);
            }
        }
    }
    REQUIRE(anySwitch == true);

    if (auto cur3 = Genesis::Engine::RendererManager::GetRenderer()) cur3->Shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
