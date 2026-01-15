#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/SubsystemRegistry.h"
#include "engine/IAudio.h"
#include "engine/Scene.h"
#include "engine/EditorHelpers.h"
#include "engine/Components.h"

#include <string>

using namespace Genesis::Engine;

class TestAudioBackend : public IAudio {
public:
    TestAudioBackend() {}
    bool Init() override { return true; }
    void Update(double) override {}
    void Shutdown() override {}
    std::string Name() const override { return "test"; }

    bool PlayOneShot(const std::string& assetPath, float volume = 1.0f) override {
        lastPath = assetPath;
        lastVolume = volume;
        played = true;
        return true;
    }
    void StopAll() override { stopped = true; }

    // Test access
    std::string lastPath;
    float lastVolume = 0.0f;
    bool played = false;
    bool stopped = false;
};

TEST_CASE("EditorHelpers: Assign audio from content and preview", "[editor][audio][preview]") {
    Genesis::Engine::Init();

    // Register test audio backend factory
    SubsystemRegistry::Instance().RegisterFactory("Audio", "test", []() {
        return std::make_unique<TestAudioBackend>();
    });

    REQUIRE(CreateAudioSubsystem("test") == true);
    auto a = GetAudioSubsystem();
    REQUIRE(a != nullptr);

    Scene scene;
    auto e = scene.Registry().create();

    bool assigned = AssignContentItemToAudioComponent(scene.Registry(), e, "Assets/sounds/test.wav");
    REQUIRE(assigned == true);
    REQUIRE(scene.Registry().all_of<AudioComponent>(e));
    auto& ac = scene.Registry().get<AudioComponent>(e);
    REQUIRE(ac.soundPath == "Assets/sounds/test.wav");

    // Play preview
    bool ok = PlayAudioPreview(ac.soundPath, ac.volume);
    REQUIRE(ok == true);

    // Check the test backend recorded the call
    auto tb = static_cast<TestAudioBackend*>(GetAudioSubsystem().get());
    REQUIRE(tb->played == true);
    REQUIRE(tb->lastPath == ac.soundPath);
}

TEST_CASE("EditorHelpers: Particle preview start/stop", "[editor][particle][preview]") {
    Scene scene;
    auto e = scene.Registry().create();
    ParticleSystemComponent p;
    p.duration = 2.0f;
    scene.Registry().emplace<ParticleSystemComponent>(e, p);

    REQUIRE(IsParticlePreviewPlaying(scene.Registry(), e) == false);
    REQUIRE(StartParticlePreview(scene.Registry(), e) == true);
    REQUIRE(IsParticlePreviewPlaying(scene.Registry(), e) == true);
    REQUIRE(StopParticlePreview(scene.Registry(), e) == true);
    REQUIRE(IsParticlePreviewPlaying(scene.Registry(), e) == false);
}
