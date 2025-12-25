#include "engine/Scene.h"
#include "engine/Components.h"
#include <iostream>

namespace Genesis::Engine {

void Scene::Update(double /*dt*/) {
    // placeholder for systems (physics, animation, etc.)
}

void Scene::Render() {
    // For all entities with ModelComponent, call Draw()
    auto view = m_registry.view<ModelComponent>();
    for (auto entity : view) {
        auto &mc = view.get<ModelComponent>(entity);
        if (mc.model)
            mc.model->Draw();
    }
}

} // namespace Genesis::Engine
