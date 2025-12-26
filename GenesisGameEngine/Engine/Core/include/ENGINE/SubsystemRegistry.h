#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/ISubsystem.h"

namespace Genesis::Engine {

class SubsystemRegistry {
public:
    using Factory = std::function<std::unique_ptr<ISubsystem>()>;

    static SubsystemRegistry& Instance();

    void RegisterFactory(const std::string& subsystemType, const std::string& name, Factory factory);

    // Returns a unique_ptr to a new subsystem instance or nullptr if not found
    std::unique_ptr<ISubsystem> Create(const std::string& subsystemType, const std::string& name) const;

    std::vector<std::string> ListBackends(const std::string& subsystemType) const;
    std::vector<std::string> ListTypes() const;

private:
    SubsystemRegistry() = default;

    // map<subsystemType, map<backendName, factory>>
    std::map<std::string, std::map<std::string, Factory>> m_factories;
};

} // namespace Genesis::Engine
