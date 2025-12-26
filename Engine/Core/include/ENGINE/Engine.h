#pragma once

#include <string>
#include <memory>

namespace Genesis::Engine {

class SubsystemManager;
class IShaderSubsystem;

bool Init(const std::string& config = "");
void Shutdown();

// Subsystem accessors
SubsystemManager& Subsystems();

// Shader subsystem helpers
bool CreateShaderSubsystem(const std::string& name);
std::shared_ptr<IShaderSubsystem> GetShaderSubsystem();

}
