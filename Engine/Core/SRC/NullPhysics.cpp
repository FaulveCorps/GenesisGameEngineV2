#include "engine/IPhysics.h"
#include "engine/SubsystemRegistry.h"

#include <iostream>

namespace Genesis::Engine {

class NullPhysics : public IPhysics {
public:
    bool Init() override { std::cout << "NullPhysics: Init\n"; return true; }
    void Shutdown() override { std::cout << "NullPhysics: Shutdown\n"; }
    void Update(double /*dt*/) override {}
    std::string Name() const override { return "null"; }
    void StepSimulation(float /*dt*/, int /*maxSubSteps*/ = 1) override {}
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
