#pragma once
#include "engine/Scene.h"
#include <entt/entt.hpp>

namespace Genesis::Editor {
    class SceneHierarchyPanel {
    public:
        SceneHierarchyPanel() = default;
        void SetContext(Engine::Scene* scene);
        void SetSelectedEntity(entt::entity entity);
        entt::entity GetSelectedEntity() const { return m_SelectionContext; }

        void OnImGuiRender();

    private:
        Engine::Scene* m_Context = nullptr;
        entt::entity m_SelectionContext = entt::null;
        
        entt::entity m_RenameEntity = entt::null;
        char m_RenameBuf[128] = "";
        
        // Internal state for modal status
        bool m_OpenRenamePopup = false;
    };
}
