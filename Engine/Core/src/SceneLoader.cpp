#include "engine/SceneLoader.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/Model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace Genesis::Engine {

bool SceneLoader::LoadScene(Scene& scene, const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // Silent fail is okay, caller handles fallback
        return false;
    }

    scene.Clear();

    std::string line;
    entt::entity currentEntity = entt::null;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "ENTITY") {
            currentEntity = scene.Registry().create();
        }
        else if (token == "NAME" && currentEntity != entt::null) {
            // NAME may contain spaces. Read the rest of the line.
            std::string rest;
            std::getline(ss, rest);
            // Trim leading spaces.
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(rest.begin());
            scene.Registry().emplace_or_replace<NameComponent>(currentEntity, NameComponent{rest});
        }
        else if (token == "TRANSFORM" && currentEntity != entt::null) {
            Transform t;
            ss >> t.x >> t.y >> t.z >> t.rx >> t.ry >> t.rz >> t.sx >> t.sy >> t.sz;
            scene.Registry().emplace<Transform>(currentEntity, t);
        }
        else if (token == "MODEL" && currentEntity != entt::null) {
            std::string path;
            ss >> path;
            auto model = std::make_shared<Model>();
            if (model->Load(path)) {
                ModelComponent mc;
                mc.model = model;
                mc.sourcePath = path;
                scene.Registry().emplace<ModelComponent>(currentEntity, mc);
            } else {
                std::cerr << "SceneLoader: Failed to load model " << path << std::endl;
            }
        }
        else if (token == "LIGHT" && currentEntity != entt::null) {
            LightComponent l;
            int type;
            ss >> type >> l.color[0] >> l.color[1] >> l.color[2] >> l.intensity;
            l.type = (LightType)type;
            if (l.type == LightType::Point) {
                if (!(ss >> l.range)) l.range = 10.0f;
            }
            scene.Registry().emplace<LightComponent>(currentEntity, l);
        }
        else if (token == "CAMERA" && currentEntity != entt::null) {
            CameraComponent c;
            int prim;
            ss >> c.fov >> c.nearPlane >> c.farPlane >> prim;
            c.primary = (prim != 0);
            scene.Registry().emplace<CameraComponent>(currentEntity, c);
        }
    }
    
    std::cout << "SceneLoader: Loaded scene from " << filePath << std::endl;
    return true;
}

bool SceneLoader::SaveScene(const Scene& scene, const std::string& filePath) {
    namespace fs = std::filesystem;
    try {
        fs::path outPath(filePath);
        if (outPath.has_parent_path()) {
            fs::create_directories(outPath.parent_path());
        }

        std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "SceneLoader: Failed to open for write: " << filePath << std::endl;
            return false;
        }

        file << "# Saved Scene\n";
        file << std::fixed << std::setprecision(6);

        const auto& reg = scene.Registry();

        // Note: entt registry iteration order is stable per run, but not guaranteed across runs.
        // For now, we just serialize in registry order.
        reg.each([&](auto entity) {
            file << "ENTITY\n";
            if (reg.any_of<NameComponent>(entity)) {
                const auto& nc = reg.get<NameComponent>(entity);
                if (!nc.name.empty()) {
                    file << "NAME " << nc.name << "\n";
                }
            }

            if (reg.any_of<Transform>(entity)) {
                const auto& t = reg.get<Transform>(entity);
                file << "TRANSFORM "
                     << t.x << " " << t.y << " " << t.z << " "
                     << t.rx << " " << t.ry << " " << t.rz << " "
                     << t.sx << " " << t.sy << " " << t.sz << "\n";
            }

            if (reg.any_of<ModelComponent>(entity)) {
                const auto& mc = reg.get<ModelComponent>(entity);
                if (!mc.sourcePath.empty()) {
                    file << "MODEL " << mc.sourcePath << "\n";
                } else {
                    // Keep silent by default; editor can show a warning if needed.
                }
            }

            if (reg.any_of<LightComponent>(entity)) {
                const auto& l = reg.get<LightComponent>(entity);
                file << "LIGHT " << (int)l.type << " "
                     << l.color[0] << " " << l.color[1] << " " << l.color[2] << " "
                     << l.intensity;
                if (l.type == LightType::Point) {
                    file << " " << l.range;
                }
                file << "\n";
            }

            if (reg.any_of<CameraComponent>(entity)) {
                const auto& c = reg.get<CameraComponent>(entity);
                file << "CAMERA " 
                     << c.fov << " " 
                     << c.nearPlane << " " 
                     << c.farPlane << " " 
                     << (c.primary ? 1 : 0) << "\n";
            }

            file << "\n";
        });

        std::cout << "SceneLoader: Saved scene to " << filePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SceneLoader: SaveScene failed: " << e.what() << std::endl;
        return false;
    }
}

}
