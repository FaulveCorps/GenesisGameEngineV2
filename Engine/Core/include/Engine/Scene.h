#pragma once

#include <memory>
#include <entt/entt.hpp>
#include "Engine/MathUtils.h"

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

    // Hierarchy helper: compute world matrix for an entity.
    Matrix4 GetWorldMatrix(entt::entity entity) const;

    // Helper to copy scene state (for Play Mode)
    void CopyFrom(const Scene& other);

    // Control whether Render() sets view/projection from the scene camera.
    void SetUseSceneCamera(bool enabled) { m_useSceneCamera = enabled; }
    bool GetUseSceneCamera() const { return m_useSceneCamera; }

private:
    entt::registry m_registry;
    bool m_runtimeActive = false;
    bool m_useSceneCamera = true;
};

} // namespace Genesis::Engine
