#pragma once

#include <string>
#include <memory>

namespace Genesis::Engine {

class SubsystemManager;
class IShaderSubsystem;
class IAudio;

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

}
