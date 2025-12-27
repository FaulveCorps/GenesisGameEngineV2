#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/INetwork.h"

TEST_CASE("Network subsystem: Null backend available", "[network]") {
    REQUIRE(Genesis::Engine::Init() == true);

    REQUIRE(Genesis::Engine::CreateNetworkSubsystem("null") == true);
    auto net = Genesis::Engine::GetNetworkSubsystem();
    REQUIRE(net != nullptr);

    REQUIRE(net->Init() == true);
    REQUIRE(net->Name() == "null");

    // Default null semantics: Host/Connect are no-ops and return false
    REQUIRE(net->Host(0) == false);
    REQUIRE(net->Connect("localhost", 12345) == false);

    net->Shutdown();
}

TEST_CASE("Network subsystem: Create invalid backend fails", "[network]") {
    REQUIRE(!Genesis::Engine::CreateNetworkSubsystem("nope"));
}
