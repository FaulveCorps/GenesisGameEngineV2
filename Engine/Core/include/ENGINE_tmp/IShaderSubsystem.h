#pragma once

#include "engine/ISubsystem.h"
#include <string>

namespace Genesis::Engine {

class IShaderSubsystem : public ISubsystem {
public:
    virtual ~IShaderSubsystem() = default;

    // Create a program from source for this backend; returns backend-native program id (0 == failure)
    virtual unsigned int CreateProgramFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) = 0;

    // Destroy a previously created program id
    virtual void DestroyProgram(unsigned int programID) = 0;
};

} // namespace Genesis::Engine
