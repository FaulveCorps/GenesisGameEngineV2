#include "catch_amalgamated.hpp"
#include "engine/Scene.h"
#include "engine/SceneLoader.h"
#include "engine/Components.h"

#include <filesystem>
#include <string>
#include <chrono>

using namespace Genesis::Engine;

static std::string UniqueTempDir_SceneTest() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto tmp = std::filesystem::temp_directory_path();
    tmp /= ("gen_scene_test_" + std::to_string(now));
    return tmp.string();
}

TEST_CASE("SceneLoader: Save/Load Audio/Particle/RigidBody roundtrip", "[scene]") {
    auto tmp = UniqueTempDir_SceneTest();
    std::filesystem::create_directories(tmp);

    Scene s1;
    auto e = s1.Registry().create();
    s1.Registry().emplace<NameComponent>(e, NameComponent{"TestEntity"});

    AudioComponent a;
    a.soundPath = "Assets/audio/sfx.wav";
    a.volume = 0.75f;
    a.pitch = 1.2f;
    a.loop = true;
    a.playOnAwake = false;
    a.spatial = true;
    a.minDistance = 0.5f;
    a.maxDistance = 12.0f;
    s1.Registry().emplace<AudioComponent>(e, a);

    ParticleSystemComponent p;
    p.duration = 10.0f;
    p.looping = false;
    p.playOnAwake = true;
    p.startLifetime = 3.4f;
    p.startSpeed = 7.1f;
    p.startSize = 0.7f;
    p.startColor[0] = 0.1f; p.startColor[1] = 0.2f; p.startColor[2] = 0.3f; p.startColor[3] = 0.4f;
    p.rateOverTime = 42.0f;
    p.emitterRadius = 1.2f;
    s1.Registry().emplace<ParticleSystemComponent>(e, p);

    RigidBodyComponent r;
    r.mass = 3.14f;
    r.useGravity = false;
    r.isKinematic = true;
    s1.Registry().emplace<RigidBodyComponent>(e, r);

    const std::string path = (std::filesystem::path(tmp) / "roundtrip.scene").string();
    REQUIRE(SceneLoader::SaveScene(s1, path) == true);

    Scene s2;
    REQUIRE(SceneLoader::LoadScene(s2, path) == true);

    bool foundA = false, foundP = false, foundR = false;
    s2.Registry().each([&](auto ent) {
        if (!foundA && s2.Registry().any_of<AudioComponent>(ent)) {
            const auto& ac = s2.Registry().get<AudioComponent>(ent);
            REQUIRE(ac.soundPath == "Assets/audio/sfx.wav");
            REQUIRE(ac.volume == Approx(0.75f));
            REQUIRE(ac.pitch == Approx(1.2f));
            REQUIRE(ac.loop == true);
            REQUIRE(ac.playOnAwake == false);
            REQUIRE(ac.spatial == true);
            REQUIRE(ac.minDistance == Approx(0.5f));
            REQUIRE(ac.maxDistance == Approx(12.0f));
            foundA = true;
        }
        if (!foundP && s2.Registry().any_of<ParticleSystemComponent>(ent)) {
            const auto& pc = s2.Registry().get<ParticleSystemComponent>(ent);
            REQUIRE(pc.duration == Approx(10.0f));
            REQUIRE(pc.looping == false);
            REQUIRE(pc.playOnAwake == true);
            REQUIRE(pc.startLifetime == Approx(3.4f));
            REQUIRE(pc.startSpeed == Approx(7.1f));
            REQUIRE(pc.startSize == Approx(0.7f));
            REQUIRE(pc.startColor[0] == Approx(0.1f));
            REQUIRE(pc.startColor[1] == Approx(0.2f));
            REQUIRE(pc.startColor[2] == Approx(0.3f));
            REQUIRE(pc.startColor[3] == Approx(0.4f));
            REQUIRE(pc.rateOverTime == Approx(42.0f));
            REQUIRE(pc.emitterRadius == Approx(1.2f));
            foundP = true;
        }
        if (!foundR && s2.Registry().any_of<RigidBodyComponent>(ent)) {
            const auto& rc = s2.Registry().get<RigidBodyComponent>(ent);
            REQUIRE(rc.mass == Approx(3.14f));
            REQUIRE(rc.useGravity == false);
            REQUIRE(rc.isKinematic == true);
            foundR = true;
        }
    });

    REQUIRE(foundA);
    REQUIRE(foundP);
    REQUIRE(foundR);

    std::filesystem::remove_all(tmp);
}
