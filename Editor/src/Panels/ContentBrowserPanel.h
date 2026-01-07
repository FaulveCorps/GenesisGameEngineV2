#pragma once
#include "engine/Scene.h"
#include <filesystem>
#include <string>
#include <entt/entt.hpp>

namespace Genesis::Editor {
    class ContentBrowserPanel {
    public:
        ContentBrowserPanel() = default;
        // Allows the panel to load scenes and update editor state
        // Return value of LoadScene callback?
        // Using function object for LoadScene might be better, but for now passing pointers is quickest step.
        void SetContext(Engine::Scene* scene, std::string* currentScenePathPtr, bool* sceneDirtyPtr, entt::entity* selectionPtr);
        void SetContext(const std::filesystem::path& assetPath);
        
        // Helper to trigger unsaved changes prompt could be passed too, but let's stick to direct loading mostly.
        // Actually, without the prompt callback, we lose safety.
        // Let's pass a std::function<bool(const std::string&)> loadSceneCallback.
        // But main.cpp logic uses PendingAction state machine.
        // Let's keep it simple: Just draw the browser and drag/drop logic. Double click loading will be handled via callback.
        
        using LoadSceneCallback = std::function<void(const std::string&)>;
        void SetLoadSceneCallback(LoadSceneCallback callback) { m_LoadSceneCallback = callback; }

        void OnImGuiRender();

    private:
        Engine::Scene* m_Scene = nullptr;
        std::string* m_CurrentScenePath = nullptr;
        bool* m_SceneDirty = nullptr;
        entt::entity* m_Selection = nullptr;
        LoadSceneCallback m_LoadSceneCallback;

        std::filesystem::path m_ContentDir = "Assets";
        std::filesystem::path m_CurrentDirectory = "Assets";
        char m_SearchBuf[128] = "";
    };
}
