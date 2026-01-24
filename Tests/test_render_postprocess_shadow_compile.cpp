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

static bool ShouldRunOpenGL() {
    const char* flag = std::getenv("GENESIS_ENABLE_OPENGL");
    return flag && std::strcmp(flag, "1") == 0;
}

static bool ResolveShaderPaths(std::string& vertPath, std::string& fragPath, const std::string& vertFile, const std::string& fragFile) {
#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif
    std::string root = PROJECT_SOURCE_DIR;
    vertPath = root + "/Assets/shaders/" + vertFile;
    fragPath = root + "/Assets/shaders/" + fragFile;

    std::ifstream v(vertPath);
    std::ifstream f(fragPath);
    if (v.good() && f.good()) {
        return true;
    }

    vertPath = "Assets/shaders/" + vertFile;
    fragPath = "Assets/shaders/" + fragFile;
    std::ifstream v2(vertPath);
    std::ifstream f2(fragPath);
    return v2.good() && f2.good();
}

static void CompileShaderPair(const std::string& vertFile, const std::string& fragFile) {
    if (!ShouldRunOpenGL()) {
        SUCCEED("OpenGL tests disabled via GENESIS_ENABLE_OPENGL");
        return;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        FAIL("SDL_Init failed");
    }

    SDL_Window* win = SDL_CreateWindow("ShaderCompileTest", 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
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

    std::string vertPath;
    std::string fragPath;
    if (!ResolveShaderPaths(vertPath, fragPath, vertFile, fragFile)) {
        SDL_GL_DestroyContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        FAIL("Shader files not found");
    }

    std::string vertSrc = ReadFile(vertPath);
    std::string fragSrc = ReadFile(fragPath);
    REQUIRE(!vertSrc.empty());
    REQUIRE(!fragSrc.empty());

    auto shader = Genesis::Engine::Shader::FromSource(vertSrc, fragSrc);
    REQUIRE(shader != nullptr);
    REQUIRE(shader->GetID() != 0);

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

TEST_CASE("Post-process shader compilation", "[shader][postprocess]") {
    CompileShaderPair("postprocess.vert", "postprocess.frag");
}

TEST_CASE("Bloom blur shader compilation", "[shader][bloom]") {
    CompileShaderPair("blur.vert", "blur.frag");
}

TEST_CASE("Shadow shader compilation", "[shader][shadow]") {
    CompileShaderPair("shadow.vert", "shadow.frag");
}
