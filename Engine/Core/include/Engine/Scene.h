#pragma once

#include <memory>
#include <entt/entt.hpp>

namespace Genesis::Engine {

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    entt::registry& Registry() { return m_registry; }
    void Clear() { m_registry.clear(); }

    void Update(double dt);
    void Render(class IGraphicsAPI* renderer);

private:
    entt::registry m_registry;
};

} // namespace Genesis::Engine
