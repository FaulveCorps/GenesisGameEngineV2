#pragma once

#include <string>
#include <entt/entt.hpp>

namespace Genesis::Engine {

class Scene;

class PrefabLoader {
public:
    static bool SavePrefab(const Scene& scene, entt::entity root, const std::string& filePath);
    static entt::entity InstantiatePrefab(Scene& scene, const std::string& filePath, entt::entity parent = entt::null);
    static bool ApplyPrefab(Scene& scene, entt::entity root);
};

} // namespace Genesis::Engine
