#include "engine/Engine.h"
#include "engine/SubsystemManager.h"
#include "engine/IShaderSubsystem.h"
#include <iostream>

namespace Genesis::Engine {

// Forward declarations for built-in subsystem registrations
void RegisterNullAudioFactory();
void RegisterNullShaderFactory();
void RegisterGLShaderFactory();

// Subsystems manager used for created subsystem instances
static SubsystemManager g_subsystems;
SubsystemManager& Subsystems() { return g_subsystems; }

// Shader subsystem handle (optional; managed by Engine)
static std::shared_ptr<IShaderSubsystem> g_shaderSubsystem;

// Create or replace the global shader subsystem instance.
// Returns true on success.
bool CreateShaderSubsystem(const std::string& name) {
    auto inst = g_subsystems.CreateSubsystem("Shader", name);
    if (!inst) return false;
    // Try to cast to IShaderSubsystem
    auto sh = std::dynamic_pointer_cast<IShaderSubsystem>(inst);
    if (!sh) {
        std::cerr << "CreateShaderSubsystem: created instance is not IShaderSubsystem" << std::endl;
        inst->Shutdown();
        return false;
    }
    g_shaderSubsystem = sh;
    std::cout << "CreateShaderSubsystem: created shader subsystem '" << name << "'" << std::endl;
    return true;
}

std::shared_ptr<IShaderSubsystem> GetShaderSubsystem() {
    return g_shaderSubsystem;
}

bool Init(const std::string& config) {
    // Ensure built-in subsystems are registered
    RegisterNullAudioFactory();
    RegisterNullShaderFactory();
    // Ensure GL factory is available when possible (explicit registration prevents static-init omission)
    RegisterGLShaderFactory();

    // Create a default null shader subsystem so code that expects one can rely on it.
    CreateShaderSubsystem("null");

    std::cout << "Genesis Engine initialized (config='" << config << "')" << std::endl;
    return true;
}

void Shutdown() {
    std::cout << "Genesis Engine shutdown" << std::endl;
}

} // namespace Genesis::Engine
