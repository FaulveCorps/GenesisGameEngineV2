#pragma once

#include <engine/Engine.h>
#include <engine/Project.h>
#include <engine/Scene.h>
#include <filesystem>
#include <engine/Window.h>
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/InspectorPanel.h"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace Genesis::Editor {

    class EditorLayer {
    public:
        EditorLayer(Genesis::Engine::Window* window);
        ~EditorLayer();

        void OnAttach();
        void OnDetach();
        void OnUpdate(float ts); // ts = timestep/deltaTime
        void OnImGuiRender();
        void OnEvent(const SDL_Event& event);
        bool IsRunning() const { return m_Running; }

    private:
        // Scene Management
        void NewScene();
        void OpenScene();
        void SaveScene();
        void SaveSceneAs();
        
        enum class PendingSceneAction {
            None,
            Quit,
            NewScene,
            LoadScenePath, // We only store the path in m_PendingLoadPath
            ShowOpenScene
        };
        bool MaybePromptUnsaved(PendingSceneAction nextAction, const std::string& nextPath = "");

        // UI Helpers
        void DrawMenuBar();
        void DrawToolbar();
        void DrawUnsavedChangesDialog();
        void DrawGizmos();
        void HandleShortcuts();
        void UpdateViewCube();

        void UI_ShowNewProjectPopup();
        void UI_ShowOpenProjectPopup();

    private:
        Genesis::Engine::Window* m_Window;

        bool m_ShowNewProjectPopup = false;
        bool m_ShowOpenProjectPopup = false;

        // Scene
        Genesis::Engine::Scene m_EditorScene;
        Genesis::Engine::Scene* m_ActiveScene; 
        // Note: Runtime scene is created/destroyed on Play/Stop. 
        // Ideally we might want a 'RuntimeScene' member but it's transient.
        // For now, m_ActiveScene points to &m_EditorScene or a temp runtime scene.
        // Actually, main.cpp uses `Genesis::Engine::Scene scene;`.
        // And when playing, `Engine::Scene* runtimeScene = new ...`.
        
        Genesis::Engine::Scene* m_RuntimeScene = nullptr;

        // Path
        std::string m_CurrentScenePath;
        bool m_SceneDirty = false;

        // Selection
        entt::entity m_SelectedEntity = entt::null;

        // Panels
        SceneHierarchyPanel m_SceneHierarchyPanel;
        ContentBrowserPanel m_ContentBrowserPanel;
        InspectorPanel m_InspectorPanel;

        // Camera
        glm::vec3 m_CameraPos = glm::vec3(0.0f, 0.0f, 10.0f);
        glm::vec3 m_CameraRot = glm::vec3(0.0f, 0.0f, 0.0f); // Pitch, Yaw, Roll
        glm::mat4 m_ViewMatrix;
        glm::mat4 m_ProjectionMatrix;
        
        // Camera Navigation
        bool m_CameraNavActive = false;
        float m_CameraSpeedBase = 15.0f;
        float m_CameraSensitivity = 0.1f;

        // Gizmos
        int m_CurrentGizmoOperation = -1; // -1 = None/Select, 7 = Translate, etc. (ImGuizmo values)
        int m_CurrentGizmoMode = 0; // Local = 0, World = 1
        bool m_UseSnap = false;
        float m_SnapValue[3] = { 0.5f, 0.5f, 0.5f };

        // Editor State
        bool m_Running = true;
        enum class SceneState { Edit, Play, Pause };
        SceneState m_SceneState = SceneState::Edit;
        bool m_ZenMode = false;
        bool m_ShowCommandPalette = false;

        // Deferred Actions
        PendingSceneAction m_PendingAction = PendingSceneAction::None;
        std::string m_PendingLoadPath; 
        bool m_ShowUnsavedChangesParams = false; // "Show" flag for popup? 
        // Actually ImGui popup state is usually managed by OpenPopup names.
        // We'll mimic main's logic.

        // ViewCube state
        bool m_ViewCubeWasActive = false;
        bool m_ViewCubeWasDragged = false;
        float m_ViewCubeDragAccumSq = 0.0f;
        bool m_ViewCubeClickArmed = false;
        glm::vec3 m_ViewCubeClickForward;
        bool m_ViewCubeAnimating = false;
        glm::vec3 m_ViewCubeStartPos, m_ViewCubeTargetPos;
        glm::quat m_ViewCubeStartRot, m_ViewCubeTargetRot;
        float m_ViewCubeAnimTime = 0.0f;

        // Command Palette
        char m_CommandSearchBuffer[128] = "";
        int m_SelectedCommandIndex = 0;

        // misc
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;
        glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
        glm::vec2 m_ViewportBounds[2]; // Min, Max

        // Profiler/Stats (if needed) - Engine has Profiler singleton
    };

}
