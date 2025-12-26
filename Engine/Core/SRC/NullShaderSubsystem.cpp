#include "engine/IShaderSubsystem.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>

namespace Genesis::Engine {

class NullShaderSubsystem : public IShaderSubsystem {
public:
    bool Init() override { std::cout << "NullShader: Init\n"; return true; }
    void Update(double /*dt*/) override {}
    void Shutdown() override { std::cout << "NullShader: Shutdown\n"; }
    std::string Name() const override { return "null"; }

    unsigned int CreateProgramFromSource(const std::string& /*vertexSrc*/, const std::string& /*fragmentSrc*/) override {
        // Null backend: pretend compilation succeeded but return 0 to indicate "no program"
        std::cout << "NullShader: CreateProgramFromSource -> returning 0 (no-op)" << std::endl;
        return 0;
    }

    void DestroyProgram(unsigned int /*programID*/) override {
        // no-op
    }
};

static bool register_null_shader = []() {
    SubsystemRegistry::Instance().RegisterFactory("Shader", "null", []() {
        return std::make_unique<NullShaderSubsystem>();
    });
    return true;
}();

// Explicit registration hook (callable by Engine::Init to force registration)
void RegisterNullShaderFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Shader", "null", []() {
        return std::make_unique<NullShaderSubsystem>();
    });
}

} // namespace Genesis::Engine
