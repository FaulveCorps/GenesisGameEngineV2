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

TEST_CASE("Box2D: joints maintain distance", "[physics][box2d][joints]") {
    REQUIRE(Genesis::Engine::Init() == true);
    REQUIRE(Genesis::Engine::CreatePhysicsSubsystem("box2d") == true);
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    REQUIRE(ph != nullptr);

    // Create two dynamic bodies and join them with a distance joint
    auto a = ph->CreateBoxRigidBody(1.0f, -0.5f, 5.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    auto b = ph->CreateBoxRigidBody(1.0f,  0.5f, 5.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(a != 0);
    REQUIRE(b != 0);

    auto j = ph->CreateDistanceJoint(a, b, -0.5f, 5.0f, 0.5f, 5.0f);
    REQUIRE(j != 0);

    // Step simulation and ensure distance is roughly preserved
    for (int i = 0; i < 120; ++i) ph->StepSimulation(1.0f/60.0f);

    float ax,ay,az,bx,by,bz;
    REQUIRE(ph->GetRigidBodyPosition(a, ax, ay, az));
    REQUIRE(ph->GetRigidBodyPosition(b, bx, by, bz));

    float dx = ax - bx;
    float dy = ay - by;
    float dist = sqrtf(dx*dx + dy*dy);
    REQUIRE(dist == Approx(1.0f).epsilon(0.1f));

    ph->DestroyRigidBody(a);
    ph->DestroyRigidBody(b);
    ph->DestroyJoint(j);
}

TEST_CASE("Box2D: contact callbacks invoked", "[physics][box2d][contacts]") {
    REQUIRE(Genesis::Engine::Init() == true);
    REQUIRE(Genesis::Engine::CreatePhysicsSubsystem("box2d") == true);
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    REQUIRE(ph != nullptr);

    // Ground
    auto g = ph->CreateBoxRigidBody(0.0f, 0.0f, 0.0f, 0.0f, 50.0f, 1.0f, 1.0f);
    REQUIRE(g != 0);
    // Dynamic body above
    auto d = ph->CreateBoxRigidBody(1.0f, 0.0f, 5.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(d != 0);

    bool sawContact = false;
    ph->SetContactCallbacks([&](BodyHandle a, BodyHandle b) { sawContact = true; }, [](BodyHandle, BodyHandle) {});

    // Step a bit to allow contact
    for (int i=0;i<240 && !sawContact;++i) ph->StepSimulation(1.0f/60.0f);

    REQUIRE(sawContact);

    ph->DestroyRigidBody(d);
    ph->DestroyRigidBody(g);
}
#else
TEST_CASE("Box2D: not available", "[physics][box2d]") {
    SUCCEED("Box2D not available; skipping tests") ;
}
#endif
