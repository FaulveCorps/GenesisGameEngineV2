#include "engine/SceneLoader.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/UI.h"
#include "engine/Model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

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
    std::unordered_map<uint64_t, entt::entity> idToEntity;
    std::vector<std::pair<entt::entity, uint64_t>> pendingParents;
    std::unordered_set<uint64_t> usedIds;
    uint64_t maxId = 0;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "ENTITY") {
            uint64_t id = 0;
            if (!(ss >> id) || id == 0) {
                id = maxId + 1;
            }
            if (usedIds.count(id) > 0) {
                uint64_t newId = maxId + 1;
                while (usedIds.count(newId) > 0) ++newId;
                std::cerr << "SceneLoader: Duplicate entity id " << id << "; remapping to " << newId << std::endl;
                id = newId;
            }
            usedIds.insert(id);
            if (id > maxId) maxId = id;

            currentEntity = scene.Registry().create();
            idToEntity[id] = currentEntity;
            scene.Registry().emplace_or_replace<StableIdComponent>(currentEntity, StableIdComponent{id});
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
                         mat.normalTextureObj = Texture::CreateFromFileAsNormalMap(mat.normalTexture);
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
        else if (token == "UI" && currentEntity != entt::null) {
            UIComponent ui;
            int type = 0;
            int useAnchors = 0;
            ss >> type
               >> ui.x >> ui.y >> ui.width >> ui.height
               >> ui.color[0] >> ui.color[1] >> ui.color[2] >> ui.color[3]
               >> useAnchors >> ui.anchorX >> ui.anchorY >> ui.pivotX >> ui.pivotY;
            ui.type = static_cast<UIType>(type);
            ui.useAnchors = (useAnchors != 0);
            float bgR = ui.backgroundColor[0];
            float bgG = ui.backgroundColor[1];
            float bgB = ui.backgroundColor[2];
            float bgA = ui.backgroundColor[3];
            int drawBg = ui.drawBackground ? 1 : 0;
            if (ss >> bgR >> bgG >> bgB >> bgA >> drawBg) {
                ui.backgroundColor[0] = bgR;
                ui.backgroundColor[1] = bgG;
                ui.backgroundColor[2] = bgB;
                ui.backgroundColor[3] = bgA;
                ui.drawBackground = (drawBg != 0);
            }
            scene.Registry().emplace_or_replace<UIComponent>(currentEntity, ui);
        }
        else if (token == "UI_TEXT" && currentEntity != entt::null) {
            std::string rest;
            std::getline(ss, rest);
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(rest.begin());
            if (auto* ui = scene.Registry().try_get<UIComponent>(currentEntity)) {
                ui->text = rest;
            }
        }
        else if (token == "UI_TEX" && currentEntity != entt::null) {
            std::string rest;
            std::getline(ss, rest);
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(rest.begin());
            if (auto* ui = scene.Registry().try_get<UIComponent>(currentEntity)) {
                if (!rest.empty() && rest != "NONE") {
                    ui->texturePath = rest;
                    ui->texture = Texture::CreateFromFile(rest);
                } else {
                    ui->texturePath.clear();
                    ui->texture.reset();
                }
            }
        }
        else if (token == "NAVGRID" && currentEntity != entt::null) {
            NavGridComponent nav;
            int autoBake = 1;
            int draw = 1;
            ss >> nav.width >> nav.height >> nav.cellSize >> nav.originX >> nav.originZ >> nav.y;
            if (ss >> autoBake >> draw) {
                nav.autoBakeColliders = (autoBake != 0);
                nav.drawDebug = (draw != 0);
            }
            int dbgPath = nav.debugPath ? 1 : 0;
            float startX = nav.debugStartX;
            float startZ = nav.debugStartZ;
            float endX = nav.debugEndX;
            float endZ = nav.debugEndZ;
            if (ss >> dbgPath >> startX >> startZ >> endX >> endZ) {
                nav.debugPath = (dbgPath != 0);
                nav.debugStartX = startX;
                nav.debugStartZ = startZ;
                nav.debugEndX = endX;
                nav.debugEndZ = endZ;
            }
            scene.Registry().emplace_or_replace<NavGridComponent>(currentEntity, nav);
        }
        else if (token == "NAVAGENT" && currentEntity != entt::null) {
            NavAgentComponent agent;
            int hasTarget = 0;
            int drawPath = 1;
            ss >> agent.speed >> agent.targetX >> agent.targetZ >> hasTarget >> agent.stopDistance >> agent.repathInterval;
            if (ss >> drawPath) {
                agent.drawPath = (drawPath != 0);
            }
            agent.hasTarget = (hasTarget != 0);
            scene.Registry().emplace_or_replace<NavAgentComponent>(currentEntity, agent);
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
        else if (token == "BOX_COLLIDER" && currentEntity != entt::null) {
            BoxColliderComponent b;
            int trigger = 0;
            ss >> b.size[0] >> b.size[1] >> b.size[2]
               >> b.offset[0] >> b.offset[1] >> b.offset[2]
               >> trigger;
            b.isTrigger = (trigger != 0);
            scene.Registry().emplace<BoxColliderComponent>(currentEntity, b);
        }
        else if (token == "SPHERE_COLLIDER" && currentEntity != entt::null) {
            SphereColliderComponent s;
            int trigger = 0;
            ss >> s.radius >> s.offset[0] >> s.offset[1] >> s.offset[2] >> trigger;
            s.isTrigger = (trigger != 0);
            scene.Registry().emplace<SphereColliderComponent>(currentEntity, s);
        }
        else if (token == "SCRIPT" && currentEntity != entt::null) {
            std::string className;
            std::getline(ss, className);
            while (!className.empty() && (className[0] == ' ' || className[0] == '\t')) className.erase(className.begin());
            if (!className.empty() && className != "NONE") {
                ScriptComponent sc;
                sc.className = className;
                sc.Instance = nullptr;
                sc.InstantiateScript = nullptr;
                sc.DestroyScript = nullptr;
                scene.Registry().emplace_or_replace<ScriptComponent>(currentEntity, sc);
            }
        }
        else if (token == "PREFAB_INSTANCE" && currentEntity != entt::null) {
            std::string path;
            int preserve = 1;
            ss >> path >> preserve;
            if (!path.empty() && path != "NONE") {
                PrefabInstanceComponent pi;
                pi.prefabPath = path;
                pi.preserveRootTransform = (preserve != 0);
                scene.Registry().emplace_or_replace<PrefabInstanceComponent>(currentEntity, pi);
            }
        }
        else if (token == "PREFAB_LINK" && currentEntity != entt::null) {
            std::string path;
            int id = -1;
            int overrideTransform = 0;
            int overrideName = 0;
            ss >> path >> id >> overrideTransform >> overrideName;
            if (!path.empty() && path != "NONE" && id >= 0) {
                PrefabLinkComponent pl;
                pl.prefabPath = path;
                pl.prefabId = id;
                pl.overrideTransform = (overrideTransform != 0);
                pl.overrideName = (overrideName != 0);
                scene.Registry().emplace_or_replace<PrefabLinkComponent>(currentEntity, pl);
            }
        }
        else if (token == "PARENT" && currentEntity != entt::null) {
            uint64_t parentId = 0;
            ss >> parentId;
            if (parentId != 0) {
                pendingParents.emplace_back(currentEntity, parentId);
            }
        }
    }

    for (const auto& [child, parentId] : pendingParents) {
        auto it = idToEntity.find(parentId);
        if (it != idToEntity.end()) {
            entt::entity parentEnt = it->second;
            if (parentEnt != entt::null && scene.Registry().valid(parentEnt)) {
                scene.Registry().emplace_or_replace<ParentComponent>(child, ParentComponent{parentEnt});
            }
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
        struct EntityEntry {
            entt::entity entity;
            uint64_t id;
        };

        std::vector<EntityEntry> ordered;
        ordered.reserve(reg.size());

        uint64_t maxIdOut = 0;
        reg.each([&](auto entity) {
            if (auto* sid = reg.try_get<StableIdComponent>(entity)) {
                if (sid->id > maxIdOut) maxIdOut = sid->id;
            }
        });

        reg.each([&](auto entity) {
            uint64_t id = 0;
            if (auto* sid = reg.try_get<StableIdComponent>(entity)) {
                id = sid->id;
            }
            if (id == 0) {
                id = ++maxIdOut;
                reg.emplace_or_replace<StableIdComponent>(entity, StableIdComponent{id});
            }
            ordered.push_back({entity, id});
        });

        std::sort(ordered.begin(), ordered.end(), [](const EntityEntry& a, const EntityEntry& b) {
            return a.id < b.id;
        });

        std::unordered_map<entt::entity, uint64_t> entityIds;
        entityIds.reserve(ordered.size());
        for (const auto& entry : ordered) {
            entityIds[entry.entity] = entry.id;
        }

        // Serialize in stable id order for deterministic diffs/merges.
        for (const auto& entry : ordered) {
            const auto entity = entry.entity;
            file << "ENTITY " << entry.id << "\n";
            if (reg.any_of<NameComponent>(entity)) {
                const auto& nc = reg.get<NameComponent>(entity);
                if (!nc.name.empty()) {
                    file << "NAME " << nc.name << "\n";
                }
            }

            if (reg.any_of<ParentComponent>(entity)) {
                const auto& pc = reg.get<ParentComponent>(entity);
                uint64_t parentId = 0;
                if (pc.parent != entt::null && reg.valid(pc.parent)) {
                    auto itParent = entityIds.find(pc.parent);
                    if (itParent != entityIds.end()) parentId = itParent->second;
                }
                if (parentId != 0) {
                    file << "PARENT " << parentId << "\n";
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

            if (reg.any_of<ScriptComponent>(entity)) {
                const auto& sc = reg.get<ScriptComponent>(entity);
                if (!sc.className.empty()) {
                    file << "SCRIPT " << sc.className << "\n";
                }
            }

            if (reg.any_of<PrefabInstanceComponent>(entity)) {
                const auto& pi = reg.get<PrefabInstanceComponent>(entity);
                if (!pi.prefabPath.empty()) {
                    file << "PREFAB_INSTANCE " << pi.prefabPath << " " << (pi.preserveRootTransform ? 1 : 0) << "\n";
                }
            }

            if (reg.any_of<PrefabLinkComponent>(entity)) {
                const auto& pl = reg.get<PrefabLinkComponent>(entity);
                if (!pl.prefabPath.empty() && pl.prefabId >= 0) {
                    file << "PREFAB_LINK " << pl.prefabPath << " " << pl.prefabId << " "
                         << (pl.overrideTransform ? 1 : 0) << " " << (pl.overrideName ? 1 : 0) << "\n";
                }
            }

            if (reg.any_of<AudioComponent>(entity)) {
                const auto& a = reg.get<AudioComponent>(entity);
                std::string path = a.soundPath.empty() ? "NONE" : a.soundPath;
                file << "AUDIO " << path << " " << a.volume << " " << a.pitch << " " << (a.loop ? 1 : 0) << " " << (a.playOnAwake ? 1 : 0) << " " << (a.spatial ? 1 : 0) << " " << a.minDistance << " " << a.maxDistance << "\n";
            }

            if (reg.any_of<UIComponent>(entity)) {
                const auto& ui = reg.get<UIComponent>(entity);
                file << "UI " << (int)ui.type << " "
                     << ui.x << " " << ui.y << " " << ui.width << " " << ui.height << " "
                     << ui.color[0] << " " << ui.color[1] << " " << ui.color[2] << " " << ui.color[3] << " "
                     << (ui.useAnchors ? 1 : 0) << " " << ui.anchorX << " " << ui.anchorY << " " << ui.pivotX << " " << ui.pivotY << " "
                     << ui.backgroundColor[0] << " " << ui.backgroundColor[1] << " " << ui.backgroundColor[2] << " " << ui.backgroundColor[3] << " "
                     << (ui.drawBackground ? 1 : 0) << "\n";
                if (!ui.text.empty()) {
                    file << "UI_TEXT " << ui.text << "\n";
                }
                if (!ui.texturePath.empty()) {
                    file << "UI_TEX " << ui.texturePath << "\n";
                }
            }

            if (reg.any_of<NavGridComponent>(entity)) {
                const auto& nav = reg.get<NavGridComponent>(entity);
                file << "NAVGRID " << nav.width << " " << nav.height << " " << nav.cellSize << " "
                     << nav.originX << " " << nav.originZ << " " << nav.y << " "
                     << (nav.autoBakeColliders ? 1 : 0) << " " << (nav.drawDebug ? 1 : 0) << " "
                     << (nav.debugPath ? 1 : 0) << " " << nav.debugStartX << " " << nav.debugStartZ << " "
                     << nav.debugEndX << " " << nav.debugEndZ << "\n";
            }

            if (reg.any_of<NavAgentComponent>(entity)) {
                const auto& agent = reg.get<NavAgentComponent>(entity);
                file << "NAVAGENT " << agent.speed << " " << agent.targetX << " " << agent.targetZ << " "
                     << (agent.hasTarget ? 1 : 0) << " " << agent.stopDistance << " " << agent.repathInterval << " "
                     << (agent.drawPath ? 1 : 0) << "\n";
            }

            if (reg.any_of<ParticleSystemComponent>(entity)) {
                const auto& p = reg.get<ParticleSystemComponent>(entity);
                file << "PARTICLE " << p.duration << " " << (p.looping ? 1 : 0) << " " << (p.playOnAwake ? 1 : 0) << " " << p.startLifetime << " " << p.startSpeed << " " << p.startSize << " " << p.startColor[0] << " " << p.startColor[1] << " " << p.startColor[2] << " " << p.startColor[3] << " " << p.rateOverTime << " " << p.emitterRadius << "\n";
            }

            if (reg.any_of<RigidBodyComponent>(entity)) {
                const auto& r = reg.get<RigidBodyComponent>(entity);
                file << "RIGIDBODY " << r.mass << " " << (r.useGravity ? 1 : 0) << " " << (r.isKinematic ? 1 : 0) << "\n";
            }

            if (reg.any_of<BoxColliderComponent>(entity)) {
                const auto& b = reg.get<BoxColliderComponent>(entity);
                file << "BOX_COLLIDER "
                     << b.size[0] << " " << b.size[1] << " " << b.size[2] << " "
                     << b.offset[0] << " " << b.offset[1] << " " << b.offset[2] << " "
                     << (b.isTrigger ? 1 : 0) << "\n";
            }

            if (reg.any_of<SphereColliderComponent>(entity)) {
                const auto& s = reg.get<SphereColliderComponent>(entity);
                file << "SPHERE_COLLIDER "
                     << s.radius << " "
                     << s.offset[0] << " " << s.offset[1] << " " << s.offset[2] << " "
                     << (s.isTrigger ? 1 : 0) << "\n";
            }

            file << "\n";
        }

        std::cout << "SceneLoader: Saved scene to " << filePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SceneLoader: SaveScene failed: " << e.what() << std::endl;
        return false;
    }
}

}
