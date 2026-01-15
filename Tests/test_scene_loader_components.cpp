#include "catch_amalgamated.hpp"
#include "engine/Scene.h"
#include "engine/SceneLoader.h"
#include "engine/Components.h"

#include <filesystem>
#include <string>
#include <chrono>

using namespace Genesis::Engine;

static std::string UniqueTempFile() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto tmp = std::filesystem::temp_directory_path();
    tmp /= ("gen_scene_test_" + std::to_string(now) + ".scene");
    return tmp.string();
}

TEST_CASE("SceneLoader: Save/Load components (physics/audio/particle)", "[scene][loader][components]") {
    Scene scene;
    auto e = scene.Registry().create();

    RigidBodyComponent rb;
    rb.mass = 5.5f; rb.useGravity = false; rb.isKinematic = true;
    scene.Registry().emplace<RigidBodyComponent>(e, rb);

    BoxColliderComponent bc;
    bc.size[0] = 1.1f; bc.size[1] = 2.2f; bc.size[2] = 3.3f;
    bc.offset[0] = 0.5f; bc.offset[1] = 0.6f; bc.offset[2] = 0.7f;
    bc.isTrigger = true;
    scene.Registry().emplace<BoxColliderComponent>(e, bc);

    SphereColliderComponent sc;
    sc.radius = 2.5f;
    sc.offset[0] = 0.2f; sc.offset[1] = 0.3f; sc.offset[2] = 0.4f;
    sc.isTrigger = false;
    scene.Registry().emplace<SphereColliderComponent>(e, sc);

    AudioComponent ac;
    ac.soundPath = "Assets/sounds/test.wav";
    ac.volume = 0.45f; ac.pitch = 1.2f; ac.loop = true; ac.spatial = true; ac.minDistance = 1.0f; ac.maxDistance = 10.0f; ac.playOnAwake = false;
    scene.Registry().emplace<AudioComponent>(e, ac);

    ParticleSystemComponent pc;
    pc.duration = 3.5f; pc.looping = false; pc.playOnAwake = true;
    pc.startLifetime = 2.25f; pc.startSpeed = 3.5f; pc.startSize = 0.75f;
    pc.startColor[0] = 0.1f; pc.startColor[1] = 0.2f; pc.startColor[2] = 0.3f; pc.startColor[3] = 0.4f;
    pc.rateOverTime = 12.0f; pc.emitterRadius = 0.25f;
    scene.Registry().emplace<ParticleSystemComponent>(e, pc);

    auto path = UniqueTempFile();
    REQUIRE(SceneLoader::SaveScene(scene, path) == true);

    Scene out;
    REQUIRE(SceneLoader::LoadScene(out, path) == true);

    bool found = false;
    out.Registry().each([&](auto entity) {
        if (out.Registry().any_of<RigidBodyComponent>(entity)) {
            found = true;
            auto& r2 = out.Registry().get<RigidBodyComponent>(entity);
            REQUIRE(r2.mass == Catch::Approx(rb.mass));
            REQUIRE(r2.useGravity == rb.useGravity);
            REQUIRE(r2.isKinematic == rb.isKinematic);

            auto& b2 = out.Registry().get<BoxColliderComponent>(entity);
            REQUIRE(b2.size[0] == Catch::Approx(bc.size[0]));
            REQUIRE(b2.size[1] == Catch::Approx(bc.size[1]));
            REQUIRE(b2.isTrigger == bc.isTrigger);

            auto& s2 = out.Registry().get<SphereColliderComponent>(entity);
            REQUIRE(s2.radius == Catch::Approx(sc.radius));

            auto& a2 = out.Registry().get<AudioComponent>(entity);
            REQUIRE(a2.soundPath == ac.soundPath);
            REQUIRE(a2.volume == Catch::Approx(ac.volume));

            auto& p2 = out.Registry().get<ParticleSystemComponent>(entity);
            REQUIRE(p2.startLifetime == Catch::Approx(pc.startLifetime));
            REQUIRE(p2.rateOverTime == Catch::Approx(pc.rateOverTime));
        }
    });

    REQUIRE(found == true);

    std::filesystem::remove(path);
}
