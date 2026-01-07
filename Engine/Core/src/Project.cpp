#include "Engine/Project.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace Genesis::Engine {

std::shared_ptr<Project> Project::s_ActiveProject;

Project::Project() {
}

std::shared_ptr<Project> Project::GetActive() {
    return s_ActiveProject;
}

void Project::SetActive(std::shared_ptr<Project> project) {
    s_ActiveProject = project;
}

std::filesystem::path Project::GetAssetDirectory() const {
    // AssetDirectory is relative to the project directory (.genesis file location)
    if (m_Config.AssetDirectory.is_absolute()) {
        return m_Config.AssetDirectory;
    }
    return m_ProjectDirectory / m_Config.AssetDirectory;
}

std::shared_ptr<Project> Project::Load(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open project file: " << path << std::endl;
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError()) {
        std::cerr << "Failed to parse project file: " << path << std::endl;
        return nullptr;
    }

    std::shared_ptr<Project> project = std::make_shared<Project>();

    const rapidjson::Value* configRoot = &doc;
    if (doc.HasMember("Project") && doc["Project"].IsObject()) {
        configRoot = &doc["Project"];
    }
    
    if (configRoot->HasMember("Name") && (*configRoot)["Name"].IsString()) {
        project->m_Config.Name = (*configRoot)["Name"].GetString();
    }

    if (configRoot->HasMember("AssetDirectory") && (*configRoot)["AssetDirectory"].IsString()) {
        project->m_Config.AssetDirectory = (*configRoot)["AssetDirectory"].GetString();
    }

    if (configRoot->HasMember("StartScene") && (*configRoot)["StartScene"].IsString()) {
        project->m_Config.StartScene = (*configRoot)["StartScene"].GetString();
    }

    project->m_ProjectDirectory = path.parent_path();

    return project;
}

bool Project::SaveActive(const std::filesystem::path& path) {
    if (s_ActiveProject) {
        return s_ActiveProject->Save(path);
    }
    return false;
}

bool Project::Save(const std::filesystem::path& path) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("Name", rapidjson::Value(m_Config.Name.c_str(), allocator), allocator);
    doc.AddMember("AssetDirectory", rapidjson::Value(m_Config.AssetDirectory.string().c_str(), allocator), allocator);
    doc.AddMember("StartScene", rapidjson::Value(m_Config.StartScene.c_str(), allocator), allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for saving: " << path << std::endl;
        return false;
    }

    file << buffer.GetString();
    return true;
}

}
