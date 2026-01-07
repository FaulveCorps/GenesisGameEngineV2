#pragma once

#include "engine/Scene.h"
#include <entt/entt.hpp>

namespace Genesis::Editor {

    class InspectorPanel {
    public:
        InspectorPanel() = default;
        void OnImGuiRender();
        void SetSelectedEntity(entt::entity entity);
        void SetContext(Genesis::Engine::Scene* scene);

    private:
        void DrawComponents(entt::entity entity);

        Genesis::Engine::Scene* m_Context = nullptr;
        entt::entity m_SelectionContext = entt::null;
    };

}
