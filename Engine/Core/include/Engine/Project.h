#pragma once

#include <string>
#include <filesystem>
#include <memory>

namespace Genesis::Engine {

struct ProjectConfig {
    std::string Name;
    std::filesystem::path AssetDirectory;
    std::string StartScene;
};

class Project {
public:
    Project();
    ~Project() = default;

    const ProjectConfig& GetConfig() const { return m_Config; }
    ProjectConfig& GetConfig() { return m_Config; }
    
    std::filesystem::path GetProjectDirectory() const { return m_ProjectDirectory; }
    std::filesystem::path GetAssetDirectory() const;

    static std::shared_ptr<Project> GetActive();
    static void SetActive(std::shared_ptr<Project> project);

    static std::shared_ptr<Project> Load(const std::filesystem::path& path);
    static bool SaveActive(const std::filesystem::path& path);
    
    // Additional helper if we want to save a specific project, 
    // but the requirement implementation details were sparse.
    // "Save: Serialize config to JSON." 
    // Usually convenient to have:
    bool Save(const std::filesystem::path& path);

private:
    ProjectConfig m_Config;
    std::filesystem::path m_ProjectDirectory;

    static std::shared_ptr<Project> s_ActiveProject;
};

}
