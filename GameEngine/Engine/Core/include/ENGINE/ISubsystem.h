#pragma once

#include <string>

namespace Genesis::Engine {

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    // Initialize subsystem (load resources, start threads, etc.)
    virtual bool Init() = 0;

    // Called each frame (or tick) to allow the subsystem to update
    virtual void Update(double dt) = 0;

    // Shutdown and free resources
    virtual void Shutdown() = 0;

    // Human readable name for the instance/backend
    virtual std::string Name() const = 0;
};

} // namespace Genesis::Engine
