#include "SceneHierarchyPanel.h"
#include "engine/Components.h"
#include "imgui.h"
#include <iostream>
#include <string>

namespace Genesis::Editor {

    void SceneHierarchyPanel::SetContext(Engine::Scene* scene) {
        m_Context = scene;
    }

    void SceneHierarchyPanel::SetSelectedEntity(entt::entity entity) {
        m_SelectionContext = entity;
    }

    void SceneHierarchyPanel::OnImGuiRender() {
        ImGui::Begin("Scene Hierarchy");
        if (!m_Context) {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Create Entity")) {
            auto e = m_Context->Registry().create();
            m_Context->Registry().emplace<Engine::NameComponent>(e, Engine::NameComponent{"Entity " + std::to_string((uint32_t)e)});
            m_Context->Registry().emplace<Engine::Transform>(e);
            m_SelectionContext = e;
        }
        ImGui::Separator();

        // F2 focuses rename for selected entity
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2)) {
            if (m_SelectionContext != entt::null && m_Context->Registry().valid(m_SelectionContext)) {
                m_RenameEntity = m_SelectionContext;
                std::string label;
                if (m_Context->Registry().any_of<Engine::NameComponent>(m_SelectionContext)) {
                    const auto& nc = m_Context->Registry().get<Engine::NameComponent>(m_SelectionContext);
                    label = nc.name.empty() ? ("Entity " + std::to_string((uint32_t)m_SelectionContext)) : nc.name;
                } else {
                    label = "Entity " + std::to_string((uint32_t)m_SelectionContext);
                }
                strncpy_s(m_RenameBuf, label.c_str(), sizeof(m_RenameBuf) - 1);
                ImGui::OpenPopup("Rename Entity");
            }
        }

        m_Context->Registry().each([&](auto entity) {
            std::string label;
            if (m_Context->Registry().any_of<Engine::NameComponent>(entity)) {
                const auto& nc = m_Context->Registry().get<Engine::NameComponent>(entity);
                label = nc.name.empty() ? ("Entity " + std::to_string((uint32_t)entity)) : nc.name;
            } else {
                label = "Entity " + std::to_string((uint32_t)entity);
            }
            
            ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
            flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
            flags |= ImGuiTreeNodeFlags_Leaf; // Assuming flat hierarchy for now

            bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", label.c_str());
            if (ImGui::IsItemClicked()) {
                m_SelectionContext = entity;
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_RenameEntity = entity;
                std::string curName = label;
                strncpy_s(m_RenameBuf, curName.c_str(), sizeof(m_RenameBuf) - 1);
                ImGui::OpenPopup("Rename Entity");
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Rename", "F2")) {
                    m_RenameEntity = entity;
                    std::string curName = label;
                    strncpy_s(m_RenameBuf, curName.c_str(), sizeof(m_RenameBuf) - 1);
                    ImGui::OpenPopup("Rename Entity");
                }
                if (ImGui::MenuItem("Duplicate")) {
                    auto dup = m_Context->Registry().create();
                    if (m_Context->Registry().any_of<Engine::NameComponent>(entity)) {
                        auto nc = m_Context->Registry().get<Engine::NameComponent>(entity);
                        if (!nc.name.empty()) nc.name += " Copy";
                        m_Context->Registry().emplace<Engine::NameComponent>(dup, nc);
                    }
                    if (m_Context->Registry().any_of<Engine::Transform>(entity)) {
                        m_Context->Registry().emplace<Engine::Transform>(dup, m_Context->Registry().get<Engine::Transform>(entity));
                    }
                    if (m_Context->Registry().any_of<Engine::LightComponent>(entity)) {
                        m_Context->Registry().emplace<Engine::LightComponent>(dup, m_Context->Registry().get<Engine::LightComponent>(entity));
                    }
                    if (m_Context->Registry().any_of<Engine::ModelComponent>(entity)) {
                        m_Context->Registry().emplace<Engine::ModelComponent>(dup, m_Context->Registry().get<Engine::ModelComponent>(entity));
                    }
                    if (m_Context->Registry().any_of<Engine::ScriptComponent>(entity)) {
                        m_Context->Registry().emplace<Engine::ScriptComponent>(dup, m_Context->Registry().get<Engine::ScriptComponent>(entity));
                    }
                    m_SelectionContext = dup;
                }
                if (ImGui::MenuItem("Delete", "Del")) {
                    if (m_Context->Registry().valid(entity)) {
                        m_Context->Registry().destroy(entity);
                        if (m_SelectionContext == entity) m_SelectionContext = entt::null;
                    }
                }
                ImGui::EndPopup();
            }

            if (opened) {
                ImGui::TreePop();
            }
        });

        if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Name:");
            ImGui::PushItemWidth(300.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::InputText("##rename", m_RenameBuf, sizeof(m_RenameBuf));
            ImGui::PopItemWidth();

            bool commit = ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter);
            ImGui::SameLine();
            bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

            if (commit) {
                if (m_RenameEntity != entt::null && m_Context->Registry().valid(m_RenameEntity)) {
                    m_Context->Registry().emplace_or_replace<Engine::NameComponent>(m_RenameEntity, Engine::NameComponent{std::string(m_RenameBuf)});
                }
                m_RenameEntity = entt::null;
                ImGui::CloseCurrentPopup();
            }
            if (cancel) {
                m_RenameEntity = entt::null;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

}
