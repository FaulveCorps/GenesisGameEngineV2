#include "engine/IPhysics.h"
#include "engine/SubsystemRegistry.h"

#include <iostream>

namespace Genesis::Engine {

class NullPhysics : public IPhysics {
public:
    bool Init() override { std::cout << "NullPhysics: Init\n"; return true; }
    void Update(double /*dt*/) override { /* no-op */ }
    void Shutdown() override { std::cout << "NullPhysics: Shutdown\n"; }
    std::string Name() const override { return "null"; }
    void StepSimulation(float /*dt*/, int /*maxSubSteps*/ = 1) override {}

    BodyHandle CreateBoxRigidBody(float /*mass*/, float /*posX*/, float /*posY*/, float /*posZ*/, float /*sizeX*/, float /*sizeY*/, float /*sizeZ*/) override { return 0; }
    void DestroyRigidBody(BodyHandle /*h*/) override {}
    bool GetRigidBodyPosition(BodyHandle /*h*/, float& /*x*/, float& /*y*/, float& /*z*/) override { return false; }
    void ApplyCentralImpulse(BodyHandle /*h*/, float /*ix*/, float /*iy*/, float /*iz*/) override {}

    // Joints (no-op)
    JointHandle CreateDistanceJoint(BodyHandle /*a*/, BodyHandle /*b*/, float /*anchorAx*/, float /*anchorAy*/, float /*anchorBx*/, float /*anchorBy*/) override { return 0; }
    JointHandle CreateRevoluteJoint(BodyHandle /*a*/, BodyHandle /*b*/, float /*anchorX*/, float /*anchorY*/) override { return 0; }
    void DestroyJoint(JointHandle /*j*/) override {}

    // Contact callbacks (ignored)
    void SetContactCallbacks(ContactCallback /*onBegin*/, ContactCallback /*onEnd*/) override {}
};

static bool register_null_physics = []() {
    SubsystemRegistry::Instance().RegisterFactory("Physics", "null", []() {
        return std::make_unique<NullPhysics>();
    });
    return true;
}();

void RegisterNullPhysicsFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Physics", "null", []() {
        return std::make_unique<NullPhysics>();
    });
}

} // namespace Genesis::Engine
