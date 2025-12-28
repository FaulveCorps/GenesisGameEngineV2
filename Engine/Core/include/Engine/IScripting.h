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
};

} // namespace Genesis::Engine
