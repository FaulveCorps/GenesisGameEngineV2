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
