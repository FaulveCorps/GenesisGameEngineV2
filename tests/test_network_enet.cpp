#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/INetwork.h"

TEST_CASE("ENet backend: init smoke test", "[network][enet]") {
#ifdef HAVE_ENET
    if (!Genesis::Engine::Init()) {
        WARN("Engine initialization failed; skipping ENet test");
        return;
    }

    bool ok = Genesis::Engine::CreateNetworkSubsystem("enet");
    if (!ok) {
        WARN("ENet backend not available at runtime; skipping test");
        return;
    }
    auto net = Genesis::Engine::GetNetworkSubsystem();
    REQUIRE(net != nullptr);
    REQUIRE(net->Init() == true);
    REQUIRE(net->Name() == "enet");
    // Do not attempt to actually open sockets in the smoke test; deeper tests require real network
    net->Shutdown();
#else
    SUCCEED("No ENet support at compile time; skipping") ;
#endif
}
