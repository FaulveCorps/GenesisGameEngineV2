#include "catch_amalgamated.hpp"
#include "engine/VulkanRenderer.h"
#include "engine/Material.h"
#include <cstdint>

namespace {
float Luminance(const float c[4]) {
    return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

bool IsInRange01(float v) {
    return v >= 0.0f && v <= 1.0f;
}
}

TEST_CASE("Vulkan stub clear color reacts to scene state", "[renderer][vulkan][unit]") {
    Genesis::Engine::VulkanRenderer renderer;

    float baseline[4] = {};
    renderer.ComputeStubClearColor(baseline);
    REQUIRE(IsInRange01(baseline[0]));
    REQUIRE(IsInRange01(baseline[1]));
    REQUIRE(IsInRange01(baseline[2]));
    REQUIRE(baseline[3] == Catch::Approx(1.0f));

    float identity[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    renderer.SetViewProjection(identity, identity);

    float dir[3] = { 0.25f, 0.35f, 1.0f };
    float color[3] = { 1.8f, 1.6f, 1.2f };
    renderer.SetGlobalLight(dir, color, 2.6f);

    Genesis::Engine::IGraphicsAPI::PointLightData point{};
    point.position[0] = 0.0f; point.position[1] = 1.0f; point.position[2] = 2.0f;
    point.color[0] = 1.0f; point.color[1] = 0.5f; point.color[2] = 0.2f;
    point.intensity = 5.0f;
    point.radius = 1.0f;
    renderer.ClearPointLights();
    renderer.AddPointLight(point);

    renderer.SetPostProcessParams(1.25f, 2.2f);
    renderer.SetShadowParams(1.0f);

    renderer.BeginFrame();
    Genesis::Engine::MeshDesc mesh;
    mesh.vertices = {
         0.0f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f
    };
    mesh.indices = { 0, 1, 2 };
    auto handle = renderer.CreateMesh(mesh);
    REQUIRE(handle.IsValid());
    renderer.DrawMesh(handle);
    renderer.DrawMesh(handle, nullptr, nullptr);
    auto* fakeTexture = reinterpret_cast<Genesis::Engine::Texture*>(static_cast<uintptr_t>(1));
    renderer.DrawTexture(fakeTexture, 8.0f, 8.0f, 32.0f, 32.0f);

    renderer.BeginFrame();
    Genesis::Engine::Material redMat;
    redMat.baseColor = { 1.0f, 0.15f, 0.10f, 1.0f };
    redMat.metallic = 0.7f;
    redMat.roughness = 0.2f;
    float translated[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        3,1,0,1
    };
    renderer.DrawMesh(handle, &redMat, translated);
    renderer.DrawTexture(fakeTexture, 0.0f, 0.0f, 128.0f, 128.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xFFFF4040u);
    float redDominant[4] = {};
    renderer.ComputeStubClearColor(redDominant);

    renderer.BeginFrame();
    Genesis::Engine::Material blueMat;
    blueMat.baseColor = { 0.10f, 0.20f, 1.0f, 1.0f };
    blueMat.metallic = 0.1f;
    blueMat.roughness = 0.85f;
    renderer.DrawMesh(handle, &blueMat, translated);
    renderer.DrawTexture(fakeTexture, 0.0f, 0.0f, 128.0f, 128.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xFF4040FFu);
    float blueDominant[4] = {};
    renderer.ComputeStubClearColor(blueDominant);
    REQUIRE(redDominant[0] > blueDominant[0]);
    REQUIRE(blueDominant[2] > redDominant[2]);

    renderer.BeginFrame();
    renderer.DrawMesh(handle);
    renderer.DrawMesh(handle, nullptr, nullptr);
    renderer.DrawTexture(fakeTexture, 8.0f, 8.0f, 32.0f, 32.0f);

    renderer.SetPostProcessBloom(false);
    renderer.SetPostProcessVignette(false, 0.0f, 1.0f, 1.0f);
    float noBloom[4] = {};
    renderer.ComputeStubClearColor(noBloom);
    REQUIRE(Luminance(noBloom) > Luminance(baseline));

    renderer.SetPostProcessBloom(true);
    renderer.SetPostProcessBloomThreshold(0.05f);
    float withBloom[4] = {};
    renderer.ComputeStubClearColor(withBloom);
    REQUIRE(Luminance(withBloom) >= Luminance(noBloom));

    renderer.SetPostProcessVignette(true, 1.0f, 0.05f, 0.1f);
    float withVignette[4] = {};
    renderer.ComputeStubClearColor(withVignette);
    REQUIRE(Luminance(withVignette) <= Luminance(withBloom));

    float movedView[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        6,2,0,1
    };
    float zoomProjection[16] = {
        1.8f,0,0,0,
        0,1.8f,0,0,
        0,0,1,0,
        0,0,0,1
    };
    renderer.SetViewProjection(movedView, zoomProjection);
    float movedCamera[4] = {};
    renderer.ComputeStubClearColor(movedCamera);
    REQUIRE(movedCamera[2] != Catch::Approx(withVignette[2]));

    renderer.DestroyMesh(handle);
}

TEST_CASE("Vulkan stub clear color reflects mesh topology attributes", "[renderer][vulkan][unit]") {
    Genesis::Engine::VulkanRenderer renderer;

    renderer.SetGlobalLight(nullptr, nullptr, 0.0f);
    renderer.ClearPointLights();
    renderer.SetPostProcessParams(1.0f, 1.0f);
    renderer.SetPostProcessBloom(false);
    renderer.SetPostProcessVignette(false, 0.0f, 1.0f, 1.0f);

    Genesis::Engine::Material mat;
    mat.baseColor = { 0.65f, 0.65f, 0.65f, 1.0f };
    mat.metallic = 0.25f;
    mat.roughness = 0.45f;

    Genesis::Engine::MeshDesc plain;
    plain.vertices = {
         0.0f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f
    };
    plain.indices = { 0, 1, 2 };

    Genesis::Engine::MeshDesc rich;
    rich.vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f
    };
    rich.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f
    };
    rich.uvs = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };
    rich.indices = { 0, 1, 2, 0, 2, 3 };

    auto plainHandle = renderer.CreateMesh(plain);
    auto richHandle = renderer.CreateMesh(rich);
    REQUIRE(plainHandle.IsValid());
    REQUIRE(richHandle.IsValid());

    renderer.BeginFrame();
    renderer.DrawMesh(plainHandle, &mat, nullptr);
    float plainColor[4] = {};
    renderer.ComputeStubClearColor(plainColor);

    renderer.BeginFrame();
    renderer.DrawMesh(richHandle, &mat, nullptr);
    float richColor[4] = {};
    renderer.ComputeStubClearColor(richColor);

    REQUIRE(Luminance(richColor) > Luminance(plainColor));
    REQUIRE(IsInRange01(richColor[0]));
    REQUIRE(IsInRange01(richColor[1]));
    REQUIRE(IsInRange01(richColor[2]));
    REQUIRE(richColor[3] == Catch::Approx(1.0f));

    renderer.DestroyMesh(plainHandle);
    renderer.DestroyMesh(richHandle);
}

TEST_CASE("Vulkan stub clear color applies UI overlay after post stack", "[renderer][vulkan][unit]") {
    Genesis::Engine::VulkanRenderer renderer;

    renderer.SetGlobalLight(nullptr, nullptr, 0.2f);
    renderer.ClearPointLights();
    renderer.SetPostProcessParams(0.8f, 2.2f);
    renderer.SetPostProcessBloom(false);
    renderer.SetPostProcessVignette(true, 1.0f, 0.05f, 0.05f);

    auto* fakeTexture = reinterpret_cast<Genesis::Engine::Texture*>(static_cast<uintptr_t>(1));

    renderer.BeginFrame();
    float postOnly[4] = {};
    renderer.ComputeStubClearColor(postOnly);

    renderer.BeginFrame();
    renderer.DrawTexture(fakeTexture, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xFFFFFFFFu);
    float withWhiteOverlay[4] = {};
    renderer.ComputeStubClearColor(withWhiteOverlay);

    REQUIRE(withWhiteOverlay[0] > postOnly[0]);
    REQUIRE(withWhiteOverlay[1] > postOnly[1]);
    REQUIRE(withWhiteOverlay[2] > postOnly[2]);
    REQUIRE(withWhiteOverlay[0] >= 0.95f);
    REQUIRE(withWhiteOverlay[1] >= 0.95f);
    REQUIRE(withWhiteOverlay[2] >= 0.95f);

    renderer.BeginFrame();
    renderer.DrawTexture(fakeTexture, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xFFFF0000u);
    renderer.DrawTexture(fakeTexture, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xFF0000FFu);
    float redThenBlue[4] = {};
    renderer.ComputeStubClearColor(redThenBlue);

    renderer.BeginFrame();
    renderer.DrawTexture(fakeTexture, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xFF0000FFu);
    renderer.DrawTexture(fakeTexture, 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xFFFF0000u);
    float blueThenRed[4] = {};
    renderer.ComputeStubClearColor(blueThenRed);

    REQUIRE(redThenBlue[2] > redThenBlue[0]);
    REQUIRE(blueThenRed[0] > blueThenRed[2]);
}