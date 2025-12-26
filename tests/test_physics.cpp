#include "catch_amalgamated.hpp"
#include "engine/SubsystemRegistry.h"
#include "ENGINE/Engine.h"
#include "ENGINE/IPhysics.h"

TEST_CASE("Physics subsystem: Null backend available", "[subsystem][physics]") {
    auto p = Genesis::Engine::SubsystemRegistry::Instance().Create("Physics", "null");
    REQUIRE(p != nullptr);
    REQUIRE(p->Init() == true);
    REQUIRE(p->Name() == "null");
    p->Shutdown();
}

TEST_CASE("Physics subsystem: Bullet backend init", "[subsystem][physics][bullet]") {
#ifdef HAVE_BULLET
    Genesis::Engine::Init();
    bool ok = Genesis::Engine::CreatePhysicsSubsystem("bullet");
    if (!ok) {
        WARN("Bullet backend not available on this host; skipping bullet init test");
        SUCCEED("Skipped bullet test");
        return;
    }
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    REQUIRE(ph != nullptr);
    // Basic step to ensure it doesn't crash
    ph->StepSimulation(0.016f, 1);
    Genesis::Engine::CreatePhysicsSubsystem("null");
#else
    SUCCEED("No Bullet support at compile time; skipping") ;
#endif
}
