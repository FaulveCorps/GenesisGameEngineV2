#include "catch_amalgamated.hpp"
#include "ENGINE/Engine.h"
#include "ENGINE/IAudio.h"

TEST_CASE("Audio subsystem: Null backend create & API", "[subsystem][audio]") {
    Genesis::Engine::Init();

    bool ok = Genesis::Engine::CreateAudioSubsystem("null");
    REQUIRE(ok == true);
    auto a = Genesis::Engine::GetAudioSubsystem();
    REQUIRE(a != nullptr);
    REQUIRE(a->Name() == "null");
    // Null backend should not play anything
    REQUIRE(a->PlayOneShot("nonexistent.wav", 1.0f) == false);

    SECTION("Extended play params compile and return false") {
        Genesis::Engine::AudioPlayParams params;
        params.volume = 0.7f;
        params.pitch = 1.2f;
        params.loop = true;
        params.spatial = true;
        params.position[0] = 1.0f;
        params.position[1] = 2.0f;
        params.position[2] = 3.0f;
        params.minDistance = 2.0f;
        params.maxDistance = 5.0f;
        REQUIRE(a->PlayOneShot("nonexistent.wav", params) == false);
    }

    SECTION("Master volume clamps and stores") {
        a->SetMasterVolume(0.6f);
        REQUIRE(a->GetMasterVolume() == Catch::Approx(0.6f));

        a->SetMasterVolume(-1.0f);
        REQUIRE(a->GetMasterVolume() == Catch::Approx(0.0f));
    }
}

TEST_CASE("Audio subsystem: Miniaudio backend init", "[subsystem][audio][miniaudio]") {
#ifdef HAVE_MINIAUDIO
    Genesis::Engine::Init();

    bool ok = Genesis::Engine::CreateAudioSubsystem("miniaudio");
    if (!ok) {
        WARN("Miniaudio backend not available on this host; skipping miniaudio test");
        SUCCEED("Skipped miniaudio test");
        return;
    }

    auto a = Genesis::Engine::GetAudioSubsystem();
    REQUIRE(a != nullptr);

    // Attempting to play without an actual asset should simply return false and not crash
    bool played = a->PlayOneShot("");
    (void)played;

    // Switch back to null to clean up
    Genesis::Engine::CreateAudioSubsystem("null");
#else
    SUCCEED("No miniaudio support at compile time; skipping") ;
#endif
}
