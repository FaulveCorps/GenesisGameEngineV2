#pragma once

#include "engine/ISubsystem.h"
#include <string>

namespace Genesis::Engine {

class IScripting : public ISubsystem {
public:
    virtual ~IScripting() = default;

    // Execute Lua (or other) script from a string. Returns true on success.
    virtual bool ExecuteString(const std::string& code) = 0;

    // Execute script file. Returns true on success.
    virtual bool ExecuteFile(const std::string& path) = 0;

    // Called when the ECS creates a new ScriptComponent.
    // The implementation should load the script, create a script instance (table/class),
    // and bind it to the entityId.
    virtual void OnEntityScriptCreate(uint32_t entityId, const std::string& scriptPath) {}
    
    // Called every frame for entities that have active scripts.
    virtual void OnEntityScriptUpdate(uint32_t entityId, double dt) {}
    
    // Called when the script component is removed or entity destroyed.
    virtual void OnEntityScriptDestroy(uint32_t entityId) {}
};

} // namespace Genesis::Engine
