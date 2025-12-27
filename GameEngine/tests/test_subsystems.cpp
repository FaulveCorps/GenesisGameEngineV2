#include "catch_amalgamated.hpp"
#include "engine/SubsystemRegistry.h"

TEST_CASE("SubsystemRegistry: NullAudio backend available", "[subsystem]") {
    auto p = Genesis::Engine::SubsystemRegistry::Instance().Create("Audio", "null");
    REQUIRE(p != nullptr);
    REQUIRE(p->Init() == true);
    REQUIRE(p->Name() == "null");
    p->Shutdown();
}
