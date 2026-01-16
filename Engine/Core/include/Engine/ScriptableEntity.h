#pragma once

#include "Scene.h"
#include <entt/entt.hpp>

namespace Genesis::Engine {

class ScriptableEntity {
public:
    virtual ~ScriptableEntity() {}

    template<typename T>
    T& GetComponent() {
        return m_Scene->Registry().get<T>(m_Entity);
    }
    
    template<typename T>
    bool HasComponent() {
        return m_Scene->Registry().any_of<T>(m_Entity);
    }

protected:
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(double dt) {}

private:
    entt::entity m_Entity{entt::null};
    Scene* m_Scene = nullptr;
    
    friend class Scene;
};

} // namespace Genesis::Engine
