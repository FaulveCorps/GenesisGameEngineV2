#include "InspectorPanel.h"
#include "engine/Components.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace Genesis::Editor {

    void InspectorPanel::SetContext(Genesis::Engine::Scene* scene) {
        m_Context = scene;
    }

    void InspectorPanel::SetSelectedEntity(entt::entity entity) {
        m_SelectionContext = entity;
    }

    void InspectorPanel::OnImGuiRender() {
        ImGui::Begin("Inspector");
        if (m_SelectionContext != entt::null && m_Context && m_Context->Registry().valid(m_SelectionContext)) {
            DrawComponents(m_SelectionContext);
        } else {
            ImGui::Text("Select an entity to view details.");
        }
        ImGui::End();
    }

    void InspectorPanel::DrawComponents(entt::entity entity) {
        // Name Component
        static char nameEditBuf[256] = "";
        // Simple way to handle buffer init: check if we switched entity or just rely on immediate mode 
        // to read it back every frame if not editing. 
        // For simplicity in this refactor, we read from component first.
        
        if (m_Context->Registry().any_of<Genesis::Engine::NameComponent>(entity)) {
            auto& nc = m_Context->Registry().get<Genesis::Engine::NameComponent>(entity);
            std::string name = nc.name.empty() ? "Entity" : nc.name;
            strncpy_s(nameEditBuf, name.c_str(), sizeof(nameEditBuf) - 1);
            
            if (ImGui::InputText("Name", nameEditBuf, sizeof(nameEditBuf))) {
                 nc.name = std::string(nameEditBuf);
            }
        } else {
             ImGui::Text("Entity ID: %u", (uint32_t)entity);
        }

        ImGui::Separator();

        // TRANSFORM
        if (m_Context->Registry().all_of<Genesis::Engine::Transform>(entity)) {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& tc = m_Context->Registry().get<Genesis::Engine::Transform>(entity);
                ImGui::DragFloat3("Position", &tc.x, 0.1f);

                // Show rotation in degrees, store radians.
                float rotDeg[3] = { glm::degrees(tc.rx), glm::degrees(tc.ry), glm::degrees(tc.rz) };
                if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f)) {
                    tc.rx = glm::radians(rotDeg[0]);
                    tc.ry = glm::radians(rotDeg[1]);
                    tc.rz = glm::radians(rotDeg[2]);
                }

                ImGui::DragFloat3("Scale", &tc.sx, 0.01f, 0.0f, 1000.0f);
            }
        }

        // CAMERA - To Be Implemented 

        // LIGHT
        if (m_Context->Registry().all_of<Genesis::Engine::LightComponent>(entity)) {
            if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& lc = m_Context->Registry().get<Genesis::Engine::LightComponent>(entity);
                const char* types[] = { "Directional", "Point" };
                int currentType = (int)lc.type;
                if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
                    lc.type = (Genesis::Engine::LightType)currentType;
                }
                ImGui::ColorEdit3("Color", lc.color);
                ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 100.0f);
                if (lc.type == Genesis::Engine::LightType::Point) {
                    ImGui::DragFloat("Range", &lc.range, 0.1f, 0.0f, 1000.0f);
                }
            }
        }

        // MODEL
        if (m_Context->Registry().all_of<Genesis::Engine::ModelComponent>(entity)) {
            if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& mc = m_Context->Registry().get<Genesis::Engine::ModelComponent>(entity);
                ImGui::TextWrapped("Source: %s", mc.sourcePath.empty() ? "(unspecified)" : mc.sourcePath.c_str());

                if (ImGui::Button("Reload") && mc.model && !mc.sourcePath.empty()) {
                    mc.model->Load(mc.sourcePath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    m_Context->Registry().remove<Genesis::Engine::ModelComponent>(entity);
                }

                ImGui::TextUnformatted("Drag a model from Content Browser onto this panel to assign.");
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        const char* droppedPath = (const char*)payload->Data;
                        if (droppedPath && droppedPath[0] != 0) {
                            std::filesystem::path p(droppedPath);
                            std::string ext = p.extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                            if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                                if (!mc.model) mc.model = std::make_shared<Genesis::Engine::Model>();
                                mc.sourcePath = p.string();
                                mc.model->Load(mc.sourcePath);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }

        // SCRIPT
        if (m_Context->Registry().all_of<Genesis::Engine::ScriptComponent>(entity)) {
            if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& sc = m_Context->Registry().get<Genesis::Engine::ScriptComponent>(entity);
                char buffer[256];
                memset(buffer, 0, sizeof(buffer));
                strncpy_s(buffer, sc.scriptPath.c_str(), sizeof(buffer));
                
                if (ImGui::InputText("Script Path", buffer, sizeof(buffer))) {
                    sc.scriptPath = std::string(buffer);
                }

                // Drag Drop support for Scripts
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                         const char* droppedPath = (const char*)payload->Data;
                         std::filesystem::path p(droppedPath);
                         if (p.extension() == ".lua") {
                             sc.scriptPath = p.string();
                         }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }

        // ADD COMPONENT BUTTON
        ImGui::Separator();
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (ImGui::MenuItem("Light")) {
                if (!m_Context->Registry().all_of<Genesis::Engine::LightComponent>(entity)) {
                    m_Context->Registry().emplace<Genesis::Engine::LightComponent>(entity);
                }
            }
            if (ImGui::MenuItem("Model")) {
                if (!m_Context->Registry().all_of<Genesis::Engine::ModelComponent>(entity)) {
                    Genesis::Engine::ModelComponent mc;
                    mc.model = std::make_shared<Genesis::Engine::Model>();
                    mc.sourcePath.clear();
                    m_Context->Registry().emplace<Genesis::Engine::ModelComponent>(entity, mc);
                }
            }
            if (ImGui::MenuItem("Script")) {
                if (!m_Context->Registry().all_of<Genesis::Engine::ScriptComponent>(entity)) {
                    m_Context->Registry().emplace<Genesis::Engine::ScriptComponent>(entity);
                }
            }
            ImGui::EndPopup();
        }
    }

}
