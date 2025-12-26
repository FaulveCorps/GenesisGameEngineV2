#include "engine/Engine.h"
#include "engine/SubsystemManager.h"
#include "engine/IShaderSubsystem.h"
#include "engine/IAudio.h"
#include "engine/IPhysics.h"
#include "engine/IInput.h"
#include "engine/INetwork.h"
#include "engine/ISave.h"
#include "engine/IScripting.h"
#include <iostream>

namespace Genesis::Engine {

// Forward declarations for built-in subsystem registrations
void RegisterNullAudioFactory();
void RegisterNullShaderFactory();
void RegisterGLShaderFactory();
void RegisterMiniaudioFactory();
void RegisterNullPhysicsFactory();
void RegisterBulletFactory();
void RegisterNullInputFactory();
void RegisterSDLInputFactory();
void RegisterNullNetworkFactory();
void RegisterNullSaveFactory();
void RegisterFileSaveFactory();
void RegisterNullScriptingFactory();
void RegisterLuaScriptingFactory();

// Subsystems manager used for created subsystem instances
static SubsystemManager g_subsystems;
SubsystemManager& Subsystems() { return g_subsystems; }

// Shader subsystem handle (optional; managed by Engine)
static std::shared_ptr<IShaderSubsystem> g_shaderSubsystem;
// Audio subsystem handle (optional; managed by Engine)
static std::shared_ptr<IAudio> g_audioSubsystem;
// Physics subsystem handle (optional; managed by Engine)
static std::shared_ptr<IPhysics> g_physicsSubsystem;


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

bool CreateAudioSubsystem(const std::string& name) {
    auto inst = g_subsystems.CreateSubsystem("Audio", name);
    if (!inst) return false;
    // Try to cast to IAudio
    auto au = std::dynamic_pointer_cast<IAudio>(inst);
    if (!au) {
        std::cerr << "CreateAudioSubsystem: created instance is not IAudio" << std::endl;
        inst->Shutdown();
        return false;
    }
    g_audioSubsystem = au;
    std::cout << "CreateAudioSubsystem: created audio subsystem '" << name << "'" << std::endl;
    return true;
}

std::shared_ptr<IAudio> GetAudioSubsystem() {
    return g_audioSubsystem;
}

bool CreatePhysicsSubsystem(const std::string& name) {
    auto inst = g_subsystems.CreateSubsystem("Physics", name);
    if (!inst) return false;
    // Try to cast to IPhysics
    auto ph = std::dynamic_pointer_cast<IPhysics>(inst);
    if (!ph) {
        std::cerr << "CreatePhysicsSubsystem: created instance is not IPhysics" << std::endl;
        inst->Shutdown();
        return false;
    }
    g_physicsSubsystem = ph;
    std::cout << "CreatePhysicsSubsystem: created physics subsystem '" << name << "'" << std::endl;
    return true;
}

std::shared_ptr<IPhysics> GetPhysicsSubsystem() {
    return g_physicsSubsystem;
}

// Input subsystem handle (optional; managed by Engine)
static std::shared_ptr<IInput> g_inputSubsystem;

bool CreateInputSubsystem(const std::string& name) {
    auto inst = g_subsystems.CreateSubsystem("Input", name);
    if (!inst) return false;
    // Try to cast to IInput
    auto in = std::dynamic_pointer_cast<IInput>(inst);
    if (!in) {
        std::cerr << "CreateInputSubsystem: created instance is not IInput" << std::endl;
        inst->Shutdown();
        return false;
    }
    g_inputSubsystem = in;
    std::cout << "CreateInputSubsystem: created input subsystem '" << name << "'" << std::endl;
    return true;
}

std::shared_ptr<IInput> GetInputSubsystem() {
    return g_inputSubsystem;
}

// Network subsystem handle (optional; managed by Engine)
static std::shared_ptr<INetwork> g_networkSubsystem;

// Save subsystem handle (optional; managed by Engine)
static std::shared_ptr<ISave> g_saveSubsystem;

// Scripting subsystem handle (optional; managed by Engine)
static std::shared_ptr<IScripting> g_scriptingSubsystem;

bool CreateNetworkSubsystem(const std::string& name) {
    auto inst = g_subsystems.CreateSubsystem("Network", name);
    if (!inst) return false;
    // Try to cast to INetwork
    auto net = std::dynamic_pointer_cast<INetwork>(inst);
    if (!net) {
        std::cerr << "CreateNetworkSubsystem: created instance is not INetwork" << std::endl;
        inst->Shutdown();
        return false;
    }
    g_networkSubsystem = net;
    std::cout << "CreateNetworkSubsystem: created network subsystem '" << name << "'" << std::endl;
    return true;
}

std::shared_ptr<INetwork> GetNetworkSubsystem() {
    return g_networkSubsystem;
}

bool CreateScriptingSubsystem(const std::string& name) {
    auto inst = g_subsystems.CreateSubsystem("Scripting", name);
    if (!inst) return false;
    // Try to cast to IScripting
    auto sc = std::dynamic_pointer_cast<IScripting>(inst);
    if (!sc) {
        std::cerr << "CreateScriptingSubsystem: created instance is not IScripting" << std::endl;
        inst->Shutdown();
        return false;
    }
    g_scriptingSubsystem = sc;
    std::cout << "CreateScriptingSubsystem: created scripting subsystem '" << name << "'" << std::endl;
    return true;
}

std::shared_ptr<IScripting> GetScriptingSubsystem() {
    return g_scriptingSubsystem;
}
bool CreateSaveSubsystem(const std::string& name) {
    auto inst = g_subsystems.CreateSubsystem("Save", name);
    if (!inst) return false;
    // Try to cast to ISave
    auto sv = std::dynamic_pointer_cast<ISave>(inst);
    if (!sv) {
        std::cerr << "CreateSaveSubsystem: created instance is not ISave" << std::endl;
        inst->Shutdown();
        return false;
    }
    g_saveSubsystem = sv;
    std::cout << "CreateSaveSubsystem: created save subsystem '" << name << "'" << std::endl;
    return true;
}

std::shared_ptr<ISave> GetSaveSubsystem() {
    return g_saveSubsystem;
}

bool Init(const std::string& config) {
    // Ensure built-in subsystems are registered
    RegisterNullAudioFactory();
    RegisterNullShaderFactory();
    // Optional: register miniaudio if available
    RegisterMiniaudioFactory();
    // Ensure GL factory is available when possible (explicit registration prevents static-init omission)
    RegisterGLShaderFactory();
    // Physics
    RegisterNullPhysicsFactory();
    RegisterBulletFactory();
    // Input
    RegisterNullInputFactory();
    RegisterSDLInputFactory();
    // Network
    RegisterNullNetworkFactory();
    // Save
    RegisterNullSaveFactory();
    RegisterFileSaveFactory();
    // Scripting
    RegisterNullScriptingFactory();
    RegisterLuaScriptingFactory();

    // Create default null subsystems so code that expects them can rely on them.
    CreateShaderSubsystem("null");
    CreateAudioSubsystem("null");
    CreatePhysicsSubsystem("null");
    CreateInputSubsystem("null");
    CreateNetworkSubsystem("null");
    CreateSaveSubsystem("null");
    CreateScriptingSubsystem("null");

    std::cout << "Genesis Engine initialized (config='" << config << "')" << std::endl;
    return true;
}

void Shutdown() {
    std::cout << "Genesis Engine shutdown" << std::endl;
}

} // namespace Genesis::Engine
