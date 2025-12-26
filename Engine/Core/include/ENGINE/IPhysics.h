#pragma once

#include "engine/ISubsystem.h"
#include <string>
#include <functional>

namespace Genesis::Engine {

class IPhysics : public ISubsystem {
public:
    using BodyHandle = size_t;

    virtual ~IPhysics() = default;

    // Advance the physics simulation by dt seconds.
    virtual void StepSimulation(float dt, int maxSubSteps = 1) = 0;

    // Create a simple box rigid body. Returns a non-zero handle on success.
    virtual BodyHandle CreateBoxRigidBody(float mass, float posX, float posY, float posZ, float sizeX, float sizeY, float sizeZ) = 0;

    // Destroy a previously created rigid body (handle). Safe to call with 0 or missing handles.
    virtual void DestroyRigidBody(BodyHandle h) = 0;

    // Query the world-space position of a rigid body. Returns true when successful.
    virtual bool GetRigidBodyPosition(BodyHandle h, float& x, float& y, float& z) = 0;

    // Apply an impulse to the center of mass of the body.
    virtual void ApplyCentralImpulse(BodyHandle h, float ix, float iy, float iz) = 0;

    // Joints (optional, 2D/3D dependent)
    using JointHandle = size_t;
    using ContactCallback = std::function<void(BodyHandle a, BodyHandle b)>;

    virtual JointHandle CreateDistanceJoint(BodyHandle a, BodyHandle b, float anchorAx, float anchorAy, float anchorBx, float anchorBy) = 0;
    virtual JointHandle CreateRevoluteJoint(BodyHandle a, BodyHandle b, float anchorX, float anchorY) = 0;
    virtual void DestroyJoint(JointHandle j) = 0;

    // Contact callbacks: onBegin/onEnd called when bodies start/stop touching.
    virtual void SetContactCallbacks(ContactCallback onBegin, ContactCallback onEnd) = 0;
};

} // namespace Genesis::Engine
