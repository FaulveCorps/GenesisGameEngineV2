#include "ContentBrowserPanel.h"
#include "engine/Components.h"
#include "engine/SceneLoader.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>

namespace Genesis::Editor {

    void ContentBrowserPanel::SetContext(Engine::Scene* scene, std::string* currentScenePathPtr, bool* sceneDirtyPtr, entt::entity* selectionPtr) {
        m_Scene = scene;
        m_CurrentScenePath = currentScenePathPtr;
        m_SceneDirty = sceneDirtyPtr;
        m_Selection = selectionPtr;
    }

    void ContentBrowserPanel::SetContext(const std::filesystem::path& assetPath) {
        m_ContentDir = assetPath;
        m_CurrentDirectory = m_ContentDir;
    }

    void ContentBrowserPanel::OnImGuiRender() {
        ImGui::Begin("Content Browser");
        
        // Ensure directory exists
        if (m_ContentDir.empty() || !std::filesystem::exists(m_ContentDir)) {
             // Fallback
             if (std::filesystem::exists("Assets")) m_ContentDir = "Assets";
        }

        if (!std::filesystem::exists(m_CurrentDirectory)) {
            m_CurrentDirectory = m_ContentDir;
        }

        // Toolbar
        if (m_CurrentDirectory != m_ContentDir) {
            if (ImGui::Button("<")) {
                m_CurrentDirectory = m_CurrentDirectory.parent_path();
            }
            ImGui::SameLine();
        }
        ImGui::TextWrapped("%s", m_CurrentDirectory.string().c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##contentSearch", "Search...", m_SearchBuf, sizeof(m_SearchBuf));
        ImGui::Separator();

        float padding = 16.0f;
        float thumbnailSize = 64.0f;
        float cellSize = thumbnailSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1) columnCount = 1;

        ImGui::Columns(columnCount, 0, false);

        std::vector<std::filesystem::directory_entry> entries;
        for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            if (a.is_directory() != b.is_directory()) return a.is_directory() > b.is_directory();
            return a.path().filename().string() < b.path().filename().string();
        });

        for (const auto& entry : entries) {
            std::string path = entry.path().string();
            std::string filename = entry.path().filename().string();

            if (m_SearchBuf[0] != 0) {
                std::string fLower = filename;
                std::string sLower = m_SearchBuf;
                std::transform(fLower.begin(), fLower.end(), fLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                std::transform(sLower.begin(), sLower.end(), sLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                if (fLower.find(sLower) == std::string::npos) continue;
            }
            
            ImGui::PushID(filename.c_str());
            // Placeholder for thumbnail (transparent button)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::ImageButton(filename.c_str(), (ImTextureID)0, ImVec2(thumbnailSize, thumbnailSize));
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (entry.is_directory()) {
                    m_CurrentDirectory = entry.path();
                } else {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                    if (ext == ".scene") {
                        if (m_LoadSceneCallback) {
                            m_LoadSceneCallback(path);
                        }
                    } else if (ext == ".lua") {
                         // Open in external editor?
                         // For now, nothing.
                    }
                }
            }

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", path.c_str(), path.length() + 1);
                ImGui::EndDragDropSource();
            }

            ImGui::TextWrapped("%s", filename.c_str());
            ImGui::NextColumn();
            ImGui::PopID();
        }
        ImGui::Columns(1);
        ImGui::End();
    }
}
