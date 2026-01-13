#pragma once

#include <memory>
#include <entt/entt.hpp>

namespace Genesis::Engine {

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    entt::registry& Registry() { return m_registry; }
    const entt::registry& Registry() const { return m_registry; }
    void Clear() { m_registry.clear(); }

    void OnRuntimeStart();
    void OnRuntimeStop();

    void OnUpdateRuntime(double dt);
    void OnUpdateEditor(double dt);
    
    // Legacy / For Tests
    void Update(double dt);

    void Render(class IGraphicsAPI* renderer);

    // Helper to copy scene state (for Play Mode)
    void CopyFrom(const Scene& other);

private:
    entt::registry m_registry;
};

} // namespace Genesis::Engine
