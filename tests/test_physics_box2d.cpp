#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/IPhysics.h"

#ifdef HAVE_BOX2D
TEST_CASE("Box2D: basic drop and step", "[physics][box2d]") {
    REQUIRE(Genesis::Engine::Init() == true);

    REQUIRE(Genesis::Engine::CreatePhysicsSubsystem("box2d") == true);
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    REQUIRE(ph != nullptr);
    REQUIRE(ph->Name() == "box2d");

    auto h = ph->CreateBoxRigidBody(1.0f, 0.0f, 10.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(h != 0);

    float x,y,z;
    REQUIRE(ph->GetRigidBodyPosition(h, x, y, z));
    REQUIRE(y == Approx(10.0f).epsilon(0.1f));

    // step simulation a bit
    for (int i=0;i<60;++i) ph->StepSimulation(1.0f/60.0f);

    REQUIRE(ph->GetRigidBodyPosition(h, x, y, z));
    REQUIRE(y < 10.0f);

    ph->DestroyRigidBody(h);
}
#else
TEST_CASE("Box2D: not available", "[physics][box2d]") {
    SUCCEED("Box2D not available; skipping tests") ;
}
#endif
