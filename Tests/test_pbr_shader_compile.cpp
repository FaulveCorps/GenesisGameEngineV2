#include "catch_amalgamated.hpp"
#include "engine/Shader.h"
#include <SDL.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <iostream>

static std::string ReadFile(const std::string& path) {
    std::ifstream t(path);
    if (!t.is_open()) return "";
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

TEST_CASE("PBR Shader Compilation", "[shader][pbr]") {
    // Setup SDL/GL context
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        FAIL("SDL_Init failed");
    }

    SDL_Window* win = SDL_CreateWindow("PBRShaderTest", 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
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

    // Check if we should run GL tests
    const char* _enableGL = std::getenv("GENESIS_ENABLE_OPENGL");
    if (!_enableGL || std::strcmp(_enableGL, "1") != 0) {
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("OpenGL tests disabled via GENESIS_ENABLE_OPENGL");
        return;
    }

    // Try to locate the shader files
#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif
    std::string root = PROJECT_SOURCE_DIR;
    std::string vertPath = root + "/Assets/shaders/pbr.vert";
    std::string fragPath = root + "/Assets/shaders/pbr.frag";
    
    std::ifstream f(vertPath);
    if (!f.good()) {
        // Fallback to relative path if macro path is invalid
        vertPath = "Assets/shaders/pbr.vert";
        fragPath = "Assets/shaders/pbr.frag";
    }

    std::cout << "Loading PBR shaders from: " << vertPath << std::endl;

    std::string vertSrc = ReadFile(vertPath);
    std::string fragSrc = ReadFile(fragPath);

    REQUIRE(!vertSrc.empty());
    REQUIRE(!fragSrc.empty());

    // Compile
    auto shader = Genesis::Engine::Shader::FromSource(vertSrc, fragSrc);
    REQUIRE(shader != nullptr);
    REQUIRE(shader->GetID() != 0);

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
