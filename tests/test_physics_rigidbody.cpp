#include "catch_amalgamated.hpp"
#include "ENGINE/Engine.h"
#include "ENGINE/IPhysics.h"

TEST_CASE("Physics rigid body: Bullet create/step/destroy", "[subsystem][physics][rigid]") {
#ifdef HAVE_BULLET
    Genesis::Engine::Init();
    bool ok = Genesis::Engine::CreatePhysicsSubsystem("bullet");
    if (!ok) {
        WARN("Bullet backend not available; skipping rigidbody test");
        SUCCEED("Skipped rigidbody test");
        return;
    }
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    REQUIRE(ph != nullptr);

    // Create a box at y=5
    IPhysics::BodyHandle h = ph->CreateBoxRigidBody(1.0f, 0.0f, 5.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(h != 0);

    float x0,y0,z0;
    bool got = ph->GetRigidBodyPosition(h, x0,y0,z0);
    REQUIRE(got == true);
    REQUIRE(y0 > 4.9f);

    // Step simulation for 1 second
    const int steps = 60;
    for (int i=0;i<steps;++i) ph->StepSimulation(1.0f/60.0f, 1);

    float x1,y1,z1;
    got = ph->GetRigidBodyPosition(h, x1,y1,z1);
    REQUIRE(got == true);
    // The body should have fallen under gravity
    REQUIRE(y1 < y0);

    ph->DestroyRigidBody(h);
    Genesis::Engine::CreatePhysicsSubsystem("null");
#else
    SUCCEED("No Bullet support at compile time; skipping") ;
#endif
}
