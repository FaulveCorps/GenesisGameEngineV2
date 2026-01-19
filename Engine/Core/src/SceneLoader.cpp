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
        else if (token == "MAT_OVERRIDE" && currentEntity != entt::null) {
            if (scene.Registry().any_of<ModelComponent>(currentEntity)) {
                auto& mc = scene.Registry().get<ModelComponent>(currentEntity);
                int index;
                Material mat;
                std::string baseTex, normTex;
                
                ss >> index 
                   >> mat.baseColor[0] >> mat.baseColor[1] >> mat.baseColor[2] >> mat.baseColor[3]
                   >> mat.metallic >> mat.roughness 
                   >> baseTex >> normTex;
                
                if (baseTex != "NONE") mat.baseColorTexture = baseTex;
                if (normTex != "NONE") mat.normalTexture = normTex;
                
                // Load overridden textures
                if (!mat.baseColorTexture.empty()) {
                   mat.baseColorTextureObj = Texture::CreateFromFile(mat.baseColorTexture);
                }
                if (!mat.normalTexture.empty()) {
                   mat.normalTextureObj = Texture::CreateFromFile(mat.normalTexture);
                }

                mc.materialOverrides[index] = mat;
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
        else if (token == "AUDIO" && currentEntity != entt::null) {
            AudioComponent a;
            std::string path;
            int loop = 0, play = 0, spatial = 0;
            ss >> path >> a.volume >> a.pitch >> loop >> play >> spatial >> a.minDistance >> a.maxDistance;
            if (path != "NONE") a.soundPath = path; else a.soundPath.clear();
            a.loop = (loop != 0);
            a.playOnAwake = (play != 0);
            a.spatial = (spatial != 0);
            scene.Registry().emplace<AudioComponent>(currentEntity, a);
        }
        else if (token == "PARTICLE" && currentEntity != entt::null) {
            ParticleSystemComponent p;
            int looping = 0, play = 0;
            ss >> p.duration >> looping >> play >> p.startLifetime >> p.startSpeed >> p.startSize >> p.startColor[0] >> p.startColor[1] >> p.startColor[2] >> p.startColor[3] >> p.rateOverTime >> p.emitterRadius;
            p.looping = (looping != 0);
            p.playOnAwake = (play != 0);
            scene.Registry().emplace<ParticleSystemComponent>(currentEntity, p);
        }
        else if (token == "RIGIDBODY" && currentEntity != entt::null) {
            RigidBodyComponent r;
            int useGrav = 1, kin = 0;
            ss >> r.mass >> useGrav >> kin;
            r.useGravity = (useGrav != 0);
            r.isKinematic = (kin != 0);
            scene.Registry().emplace<RigidBodyComponent>(currentEntity, r);
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
                    
                    // Save overrides
                    for (const auto& [idx, mat] : mc.materialOverrides) {
                        std::string baseTex = mat.baseColorTexture.empty() ? "NONE" : mat.baseColorTexture;
                        std::string normTex = mat.normalTexture.empty() ? "NONE" : mat.normalTexture;
                        
                        file << "MAT_OVERRIDE " << idx << " "
                             << mat.baseColor[0] << " " << mat.baseColor[1] << " " << mat.baseColor[2] << " " << mat.baseColor[3] << " "
                             << mat.metallic << " " << mat.roughness << " "
                             << baseTex << " " << normTex << "\n";
                    }
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

            if (reg.any_of<AudioComponent>(entity)) {
                const auto& a = reg.get<AudioComponent>(entity);
                std::string path = a.soundPath.empty() ? "NONE" : a.soundPath;
                file << "AUDIO " << path << " " << a.volume << " " << a.pitch << " " << (a.loop ? 1 : 0) << " " << (a.playOnAwake ? 1 : 0) << " " << (a.spatial ? 1 : 0) << " " << a.minDistance << " " << a.maxDistance << "\n";
            }

            if (reg.any_of<ParticleSystemComponent>(entity)) {
                const auto& p = reg.get<ParticleSystemComponent>(entity);
                file << "PARTICLE " << p.duration << " " << (p.looping ? 1 : 0) << " " << (p.playOnAwake ? 1 : 0) << " " << p.startLifetime << " " << p.startSpeed << " " << p.startSize << " " << p.startColor[0] << " " << p.startColor[1] << " " << p.startColor[2] << " " << p.startColor[3] << " " << p.rateOverTime << " " << p.emitterRadius << "\n";
            }

            if (reg.any_of<RigidBodyComponent>(entity)) {
                const auto& r = reg.get<RigidBodyComponent>(entity);
                file << "RIGIDBODY " << r.mass << " " << (r.useGravity ? 1 : 0) << " " << (r.isKinematic ? 1 : 0) << "\n";
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
