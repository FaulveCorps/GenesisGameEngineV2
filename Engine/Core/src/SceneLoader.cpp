#include "engine/SceneLoader.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/Model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

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
                scene.Registry().emplace<ModelComponent>(currentEntity, ModelComponent{model});
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
    }
    
    std::cout << "SceneLoader: Loaded scene from " << filePath << std::endl;
    return true;
}

}
