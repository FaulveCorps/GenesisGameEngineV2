#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/ISubsystem.h"
#include "engine/SubsystemRegistry.h"

namespace Genesis::Engine {

class SubsystemManager {
public:
    SubsystemManager() = default;
    ~SubsystemManager();

    // Create and initialize a subsystem instance by type and backend name.
    // Returns a shared_ptr to the instance or nullptr on failure.
    std::shared_ptr<ISubsystem> CreateSubsystem(const std::string& type, const std::string& name);

    // Shutdown and destroy all created instances
    void DestroyAll();

    const std::vector<std::shared_ptr<ISubsystem>>& Instances() const { return m_instances; }

private:
    std::vector<std::shared_ptr<ISubsystem>> m_instances;
};

} // namespace Genesis::Engine
