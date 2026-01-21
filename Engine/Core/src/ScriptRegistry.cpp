#include "Engine/ScriptRegistry.h"
#include <algorithm>

namespace Genesis::Engine {

static std::unordered_map<std::string, ScriptInfo>& Registry() {
    static std::unordered_map<std::string, ScriptInfo> registry;
    return registry;
}

void ScriptRegistry::Register(const std::string& name, ScriptableEntity* (*create)(), void (*destroy)(ScriptComponent*)) {
    if (name.empty()) return;
    Registry()[name] = ScriptInfo{ create, destroy };
}

const ScriptInfo* ScriptRegistry::Find(const std::string& name) {
    auto& reg = Registry();
    auto it = reg.find(name);
    if (it == reg.end()) return nullptr;
    return &it->second;
}

std::vector<std::string> ScriptRegistry::GetRegisteredNames() {
    std::vector<std::string> names;
    auto& reg = Registry();
    names.reserve(reg.size());
    for (const auto& entry : reg) {
        names.push_back(entry.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace Genesis::Engine
