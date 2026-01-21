#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Engine/ScriptableEntity.h"
#include "Engine/Components.h"

namespace Genesis::Engine {

struct ScriptInfo {
    ScriptableEntity* (*create)() = nullptr;
    void (*destroy)(ScriptComponent*) = nullptr;
};

class ScriptRegistry {
public:
    static void Register(const std::string& name, ScriptableEntity* (*create)(), void (*destroy)(ScriptComponent*));
    static const ScriptInfo* Find(const std::string& name);
    static std::vector<std::string> GetRegisteredNames();

    template<typename T>
    static void Register(const std::string& name) {
        Register(name,
            []() { return static_cast<ScriptableEntity*>(new T()); },
            [](ScriptComponent* sc) {
                delete sc->Instance;
                sc->Instance = nullptr;
            });
    }
};

} // namespace Genesis::Engine
