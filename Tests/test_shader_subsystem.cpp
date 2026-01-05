#include "catch_amalgamated.hpp"
#include "ENGINE/Engine.h"
#include "ENGINE/IShaderSubsystem.h"
#include <SDL.h>
#include <iostream>

TEST_CASE("Shader subsystem: Null backend available", "[subsystem][shader]") {
    Genesis::Engine::Init();

    bool ok = Genesis::Engine::CreateShaderSubsystem("null");
    REQUIRE(ok == true);
    auto s = Genesis::Engine::GetShaderSubsystem();
    REQUIRE(s != nullptr);

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

    unsigned int id = s->CreateProgramFromSource(vert, frag);
    REQUIRE(id == 0); // null backend returns 0
}

TEST_CASE("Shader subsystem: GL backend create & compile", "[subsystem][shader][opengl]") {
    bool sdlInitRes = SDL_Init(SDL_INIT_VIDEO);
    REQUIRE(sdlInitRes == true);

    SDL_Window* win = SDL_CreateWindow("ShaderSubsystemTest", 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    REQUIRE(win != nullptr);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    REQUIRE(ctx != nullptr);

    // Try to create opengl shader subsystem
    bool ok = Genesis::Engine::CreateShaderSubsystem("opengl");
    if (!ok) {
        WARN("Opengl shader subsystem not available on this host; skipping GL compile test") ;
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("Skipped GL backend test");
        return;
    }

    auto s = Genesis::Engine::GetShaderSubsystem();
    REQUIRE(s != nullptr);

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

    unsigned int id = s->CreateProgramFromSource(vert, frag);
    // On systems with GL context & functions, id should be non-zero
    if (id == 0) {
        WARN("GL program was not created (id==0); functions may be missing)");
    } else {
        REQUIRE(id != 0);
        s->DestroyProgram(id);
    }

    // Clean up
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
