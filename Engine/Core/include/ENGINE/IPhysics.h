#pragma once

#include "engine/ISubsystem.h"
#include <string>

namespace Genesis::Engine {

class IPhysics : public ISubsystem {
public:
    virtual ~IPhysics() = default;

    // Advance the physics simulation by dt seconds.
    virtual void StepSimulation(float dt, int maxSubSteps = 1) = 0;
};

} // namespace Genesis::Engine
