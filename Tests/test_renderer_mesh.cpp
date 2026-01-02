#include "catch_amalgamated.hpp"
#include "engine/OpenGLRenderer.h"
#include "engine/Material.h"
#include <SDL.h>
#include <vector>
#include <cstring>
#include <iostream>

TEST_CASE("OpenGLRenderer Mesh Lifecycle", "[renderer][mesh]") {
    // Setup SDL/GL context
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        FAIL("SDL_Init failed");
    }

    SDL_Window* win = SDL_CreateWindow("MeshTest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
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
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        SUCCEED("OpenGL tests disabled via GENESIS_ENABLE_OPENGL");
        return;
    }

    Genesis::Engine::OpenGLRenderer renderer;
    REQUIRE(renderer.Init(win, ctx));

    // Create a simple triangle mesh
    Genesis::Engine::MeshDesc desc;
    desc.vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    desc.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };
    desc.indices = { 0, 1, 2 };

    auto meshHandle = renderer.CreateMesh(desc);
    REQUIRE(meshHandle.IsValid());

    // Create a material
    Genesis::Engine::Material mat;
    mat.baseColor[0] = 1.0f;
    mat.baseColor[1] = 0.0f;
    mat.baseColor[2] = 0.0f;
    mat.baseColor[3] = 1.0f;
    mat.metallic = 0.5f;
    mat.roughness = 0.2f;

    // Draw (should not crash)
    renderer.BeginFrame();
    float transform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    renderer.DrawMesh(meshHandle, &mat, transform);
    renderer.EndFrame();

    // Destroy mesh
    renderer.DestroyMesh(meshHandle);

    renderer.Shutdown();

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
