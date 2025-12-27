#include "engine/SubsystemRegistry.h"
#include <iostream>

namespace Genesis::Engine {

SubsystemRegistry& SubsystemRegistry::Instance() {
    static SubsystemRegistry inst;
    return inst;
}

void SubsystemRegistry::RegisterFactory(const std::string& subsystemType, const std::string& name, Factory factory) {
    m_factories[subsystemType][name] = std::move(factory);
    std::cout << "SubsystemRegistry: registered '" << subsystemType << "/" << name << "'" << std::endl;
}

std::unique_ptr<ISubsystem> SubsystemRegistry::Create(const std::string& subsystemType, const std::string& name) const {
    auto it = m_factories.find(subsystemType);
    if (it == m_factories.end()) return nullptr;
    auto it2 = it->second.find(name);
    if (it2 == it->second.end()) return nullptr;
    return it2->second();
}

std::vector<std::string> SubsystemRegistry::ListBackends(const std::string& subsystemType) const {
    std::vector<std::string> out;
    auto it = m_factories.find(subsystemType);
    if (it == m_factories.end()) return out;
    for (auto const& p : it->second)
        out.push_back(p.first);
    return out;
}

std::vector<std::string> SubsystemRegistry::ListTypes() const {
    std::vector<std::string> out;
    for (auto const& p : m_factories)
        out.push_back(p.first);
    return out;
}

} // namespace Genesis::Engine
