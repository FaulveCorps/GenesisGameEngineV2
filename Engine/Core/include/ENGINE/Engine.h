#pragma once

#include <string>
#include <memory>

namespace Genesis::Engine {

class SubsystemManager;
class IShaderSubsystem;
class IAudio;
class IPhysics;
class IInput;
class INetwork;
class ISave;
class IScripting;

bool Init(const std::string& config = "");
void Shutdown();

// Subsystem accessors
SubsystemManager& Subsystems();

// Shader subsystem helpers
bool CreateShaderSubsystem(const std::string& name);
std::shared_ptr<IShaderSubsystem> GetShaderSubsystem();

// Audio subsystem helpers
bool CreateAudioSubsystem(const std::string& name);
std::shared_ptr<IAudio> GetAudioSubsystem();

// Physics subsystem helpers
bool CreatePhysicsSubsystem(const std::string& name);
std::shared_ptr<IPhysics> GetPhysicsSubsystem();

// Input subsystem helpers
bool CreateInputSubsystem(const std::string& name);
std::shared_ptr<IInput> GetInputSubsystem();

// Network subsystem helpers
bool CreateNetworkSubsystem(const std::string& name);
std::shared_ptr<INetwork> GetNetworkSubsystem();

// Save subsystem helpers
bool CreateSaveSubsystem(const std::string& name);
std::shared_ptr<ISave> GetSaveSubsystem();

// Scripting subsystem helpers
bool CreateScriptingSubsystem(const std::string& name);
std::shared_ptr<IScripting> GetScriptingSubsystem();

}
