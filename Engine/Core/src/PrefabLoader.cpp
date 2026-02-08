#include "Engine/PrefabLoader.h"
#include "Engine/Scene.h"
#include "Engine/Components.h"
#include "engine/UI.h"
#include "Engine/Model.h"
#include "Engine/Texture.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iomanip>

namespace Genesis::Engine {

struct PrefabEntityData {
    int id = -1;
    int parentId = -1;

    bool hasName = false;
    NameComponent name;

    bool hasTransform = false;
    Transform transform;

    bool hasModel = false;
    ModelComponent model;

    bool hasLight = false;
    LightComponent light;

    bool hasCamera = false;
    CameraComponent camera;

    bool hasAudio = false;
    AudioComponent audio;

    bool hasUI = false;
    UIComponent ui;

    bool hasNavGrid = false;
    NavGridComponent navGrid;

    bool hasNavAgent = false;
    NavAgentComponent navAgent;

    bool hasParticle = false;
    ParticleSystemComponent particle;

    bool hasRigidBody = false;
    RigidBodyComponent rigidBody;

    bool hasBoxCollider = false;
    BoxColliderComponent boxCollider;

    bool hasSphereCollider = false;
    SphereColliderComponent sphereCollider;

    bool hasScript = false;
    ScriptComponent script;
};

static void BuildChildrenMap(const entt::registry& reg, std::unordered_map<entt::entity, std::vector<entt::entity>>& children) {
    children.clear();
    reg.each([&](auto entity) {
        if (reg.any_of<ParentComponent>(entity)) {
            auto parent = reg.get<ParentComponent>(entity).parent;
            if (parent != entt::null && reg.valid(parent) && parent != entity) {
                children[parent].push_back(entity);
            }
        }
    });

    for (auto& entry : children) {
        std::sort(entry.second.begin(), entry.second.end());
    }
}

static void CollectSubtree(entt::entity root,
                           const std::unordered_map<entt::entity, std::vector<entt::entity>>& children,
                           std::vector<entt::entity>& out) {
    out.push_back(root);
    auto it = children.find(root);
    if (it == children.end()) return;
    for (auto child : it->second) {
        CollectSubtree(child, children, out);
    }
}

static void WriteModelOverrides(std::ofstream& file, const ModelComponent& mc) {
    for (const auto& [idx, mat] : mc.materialOverrides) {
        std::string baseTex = mat.baseColorTexture.empty() ? "NONE" : mat.baseColorTexture;
        std::string normTex = mat.normalTexture.empty() ? "NONE" : mat.normalTexture;

        file << "MAT_OVERRIDE " << idx << " "
             << mat.baseColor[0] << " " << mat.baseColor[1] << " " << mat.baseColor[2] << " " << mat.baseColor[3] << " "
             << mat.metallic << " " << mat.roughness << " "
             << baseTex << " " << normTex << "\n";
    }
}

static void ApplyPrefabData(entt::registry& reg, entt::entity entity, const PrefabEntityData& data, bool skipTransform, bool skipName) {
    if (data.hasName && !skipName) {
        reg.emplace_or_replace<NameComponent>(entity, data.name);
    } else if (!data.hasName && !skipName && reg.any_of<NameComponent>(entity)) {
        reg.remove<NameComponent>(entity);
    }

    if (data.hasTransform && !skipTransform) {
        reg.emplace_or_replace<Transform>(entity, data.transform);
    } else if (!data.hasTransform && !skipTransform && reg.any_of<Transform>(entity)) {
        reg.remove<Transform>(entity);
    }

    if (data.hasModel) {
        reg.emplace_or_replace<ModelComponent>(entity, data.model);
    } else if (reg.any_of<ModelComponent>(entity)) {
        reg.remove<ModelComponent>(entity);
    }

    if (data.hasLight) {
        reg.emplace_or_replace<LightComponent>(entity, data.light);
    } else if (reg.any_of<LightComponent>(entity)) {
        reg.remove<LightComponent>(entity);
    }

    if (data.hasCamera) {
        reg.emplace_or_replace<CameraComponent>(entity, data.camera);
    } else if (reg.any_of<CameraComponent>(entity)) {
        reg.remove<CameraComponent>(entity);
    }

    if (data.hasAudio) {
        reg.emplace_or_replace<AudioComponent>(entity, data.audio);
    } else if (reg.any_of<AudioComponent>(entity)) {
        reg.remove<AudioComponent>(entity);
    }

    if (data.hasUI) {
        reg.emplace_or_replace<UIComponent>(entity, data.ui);
    } else if (reg.any_of<UIComponent>(entity)) {
        reg.remove<UIComponent>(entity);
    }

    if (data.hasNavGrid) {
        reg.emplace_or_replace<NavGridComponent>(entity, data.navGrid);
    } else if (reg.any_of<NavGridComponent>(entity)) {
        reg.remove<NavGridComponent>(entity);
    }

    if (data.hasNavAgent) {
        reg.emplace_or_replace<NavAgentComponent>(entity, data.navAgent);
    } else if (reg.any_of<NavAgentComponent>(entity)) {
        reg.remove<NavAgentComponent>(entity);
    }

    if (data.hasParticle) {
        reg.emplace_or_replace<ParticleSystemComponent>(entity, data.particle);
    } else if (reg.any_of<ParticleSystemComponent>(entity)) {
        reg.remove<ParticleSystemComponent>(entity);
    }

    if (data.hasRigidBody) {
        reg.emplace_or_replace<RigidBodyComponent>(entity, data.rigidBody);
    } else if (reg.any_of<RigidBodyComponent>(entity)) {
        reg.remove<RigidBodyComponent>(entity);
    }

    if (data.hasBoxCollider) {
        reg.emplace_or_replace<BoxColliderComponent>(entity, data.boxCollider);
    } else if (reg.any_of<BoxColliderComponent>(entity)) {
        reg.remove<BoxColliderComponent>(entity);
    }

    if (data.hasSphereCollider) {
        reg.emplace_or_replace<SphereColliderComponent>(entity, data.sphereCollider);
    } else if (reg.any_of<SphereColliderComponent>(entity)) {
        reg.remove<SphereColliderComponent>(entity);
    }

    if (data.hasScript) {
        ScriptComponent sc = data.script;
        sc.Instance = nullptr;
        sc.InstantiateScript = nullptr;
        sc.DestroyScript = nullptr;
        reg.emplace_or_replace<ScriptComponent>(entity, sc);
    } else if (reg.any_of<ScriptComponent>(entity)) {
        reg.remove<ScriptComponent>(entity);
    }
}

static bool ParsePrefabFile(const std::string& filePath, std::vector<PrefabEntityData>& outEntities, int& outRootId) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    outEntities.clear();
    outRootId = -1;
    std::unordered_map<int, size_t> idToIndex;

    std::string line;
    PrefabEntityData* current = nullptr;
    int nextId = 0;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "ENTITY") {
            int id = -1;
            if (!(ss >> id)) {
                id = nextId;
            }
            nextId = std::max(nextId, id + 1);
            PrefabEntityData data;
            data.id = id;
            outEntities.push_back(data);
            idToIndex[id] = outEntities.size() - 1;
            current = &outEntities.back();
            if (outRootId < 0) outRootId = id;
        } else if (token == "NAME" && current) {
            std::string rest;
            std::getline(ss, rest);
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(rest.begin());
            current->hasName = true;
            current->name = NameComponent{rest};
        } else if (token == "PARENT" && current) {
            int parentId = -1;
            ss >> parentId;
            current->parentId = parentId;
        } else if (token == "TRANSFORM" && current) {
            Transform t;
            ss >> t.x >> t.y >> t.z >> t.rx >> t.ry >> t.rz >> t.sx >> t.sy >> t.sz;
            current->hasTransform = true;
            current->transform = t;
        } else if (token == "MODEL" && current) {
            std::string path;
            ss >> path;
            ModelComponent mc;
            mc.model = std::make_shared<Model>();
            mc.sourcePath = path;
            if (mc.model->Load(path)) {
                current->hasModel = true;
                current->model = mc;
            } else {
                std::cerr << "PrefabLoader: Failed to load model " << path << std::endl;
            }
        } else if (token == "MAT_OVERRIDE" && current) {
            if (current->hasModel) {
                int index;
                Material mat;
                std::string baseTex, normTex;
                ss >> index
                   >> mat.baseColor[0] >> mat.baseColor[1] >> mat.baseColor[2] >> mat.baseColor[3]
                   >> mat.metallic >> mat.roughness
                   >> baseTex >> normTex;

                if (baseTex != "NONE") mat.baseColorTexture = baseTex;
                if (normTex != "NONE") mat.normalTexture = normTex;

                if (!mat.baseColorTexture.empty()) {
                    mat.baseColorTextureObj = Texture::CreateFromFile(mat.baseColorTexture);
                }
                if (!mat.normalTexture.empty()) {
                    mat.normalTextureObj = Texture::CreateFromFileAsNormalMap(mat.normalTexture);
                }

                current->model.materialOverrides[index] = mat;
            }
        } else if (token == "LIGHT" && current) {
            LightComponent l;
            int type;
            ss >> type >> l.color[0] >> l.color[1] >> l.color[2] >> l.intensity;
            l.type = (LightType)type;
            if (l.type == LightType::Point) {
                if (!(ss >> l.range)) l.range = 10.0f;
            }
            current->hasLight = true;
            current->light = l;
        } else if (token == "CAMERA" && current) {
            CameraComponent c;
            int prim;
            ss >> c.fov >> c.nearPlane >> c.farPlane >> prim;
            c.primary = (prim != 0);
            current->hasCamera = true;
            current->camera = c;
        } else if (token == "AUDIO" && current) {
            AudioComponent a;
            std::string path;
            int loop = 0, play = 0, spatial = 0;
            ss >> path >> a.volume >> a.pitch >> loop >> play >> spatial >> a.minDistance >> a.maxDistance;
            if (path != "NONE") a.soundPath = path; else a.soundPath.clear();
            a.loop = (loop != 0);
            a.playOnAwake = (play != 0);
            a.spatial = (spatial != 0);
            current->hasAudio = true;
            current->audio = a;
        } else if (token == "UI" && current) {
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
                int useAlign = ui.useTextAlign ? 1 : 0;
                int alignH = static_cast<int>(ui.textAlignH);
                int alignV = static_cast<int>(ui.textAlignV);
                if (ss >> useAlign >> alignH >> alignV) {
                    ui.useTextAlign = (useAlign != 0);
                    ui.textAlignH = static_cast<UIAlignH>(alignH);
                    ui.textAlignV = static_cast<UIAlignV>(alignV);
                    float textScale = ui.textScale;
                    int wrapText = ui.wrapText ? 1 : 0;
                    float padX = ui.paddingX;
                    float padY = ui.paddingY;
                    if (ss >> textScale >> wrapText >> padX >> padY) {
                        ui.textScale = textScale;
                        ui.wrapText = (wrapText != 0);
                        ui.paddingX = padX;
                        ui.paddingY = padY;
                        int drawBorder = ui.drawBorder ? 1 : 0;
                        float borderThickness = ui.borderThickness;
                        float borderR = ui.borderColor[0];
                        float borderG = ui.borderColor[1];
                        float borderB = ui.borderColor[2];
                        float borderA = ui.borderColor[3];
                        if (ss >> drawBorder >> borderThickness >> borderR >> borderG >> borderB >> borderA) {
                            ui.drawBorder = (drawBorder != 0);
                            ui.borderThickness = borderThickness;
                            ui.borderColor[0] = borderR;
                            ui.borderColor[1] = borderG;
                            ui.borderColor[2] = borderB;
                            ui.borderColor[3] = borderA;
                        }
                    }
                }
            }
            current->hasUI = true;
            current->ui = ui;
        } else if (token == "UI_TEXT" && current) {
            std::string rest;
            std::getline(ss, rest);
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(rest.begin());
            current->hasUI = true;
            current->ui.text = rest;
        } else if (token == "UI_TEX" && current) {
            std::string rest;
            std::getline(ss, rest);
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest.erase(rest.begin());
            current->hasUI = true;
            if (!rest.empty() && rest != "NONE") {
                current->ui.texturePath = rest;
                current->ui.texture = Texture::CreateFromFile(rest);
            } else {
                current->ui.texturePath.clear();
                current->ui.texture.reset();
            }
        } else if (token == "NAVGRID" && current) {
            NavGridComponent nav;
            int autoBake = 1;
            int draw = 1;
            ss >> nav.width >> nav.height >> nav.cellSize >> nav.originX >> nav.originZ >> nav.y;
            if (ss >> autoBake >> draw) {
                nav.autoBakeColliders = (autoBake != 0);
                nav.drawDebug = (draw != 0);
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
            }
            current->hasNavGrid = true;
            current->navGrid = nav;
        } else if (token == "NAVAGENT" && current) {
            NavAgentComponent agent;
            int hasTarget = 0;
            int drawPath = 1;
            ss >> agent.speed >> agent.targetX >> agent.targetZ >> hasTarget >> agent.stopDistance >> agent.repathInterval;
            if (ss >> drawPath) {
                agent.drawPath = (drawPath != 0);
            }
            agent.hasTarget = (hasTarget != 0);
            current->hasNavAgent = true;
            current->navAgent = agent;
        } else if (token == "PARTICLE" && current) {
            ParticleSystemComponent p;
            int looping = 0, play = 0;
            ss >> p.duration >> looping >> play >> p.startLifetime >> p.startSpeed >> p.startSize
               >> p.startColor[0] >> p.startColor[1] >> p.startColor[2] >> p.startColor[3]
               >> p.rateOverTime >> p.emitterRadius;
            p.looping = (looping != 0);
            p.playOnAwake = (play != 0);
            current->hasParticle = true;
            current->particle = p;
        } else if (token == "RIGIDBODY" && current) {
            RigidBodyComponent r;
            int useGrav = 1, kin = 0;
            ss >> r.mass >> useGrav >> kin;
            r.useGravity = (useGrav != 0);
            r.isKinematic = (kin != 0);
            current->hasRigidBody = true;
            current->rigidBody = r;
        } else if (token == "BOX_COLLIDER" && current) {
            BoxColliderComponent b;
            int trigger = 0;
            ss >> b.size[0] >> b.size[1] >> b.size[2]
               >> b.offset[0] >> b.offset[1] >> b.offset[2]
               >> trigger;
            b.isTrigger = (trigger != 0);
            current->hasBoxCollider = true;
            current->boxCollider = b;
        } else if (token == "SPHERE_COLLIDER" && current) {
            SphereColliderComponent s;
            int trigger = 0;
            ss >> s.radius >> s.offset[0] >> s.offset[1] >> s.offset[2] >> trigger;
            s.isTrigger = (trigger != 0);
            current->hasSphereCollider = true;
            current->sphereCollider = s;
        } else if (token == "SCRIPT" && current) {
            std::string className;
            std::getline(ss, className);
            while (!className.empty() && (className[0] == ' ' || className[0] == '\t')) className.erase(className.begin());
            if (!className.empty() && className != "NONE") {
                ScriptComponent sc;
                sc.className = className;
                sc.Instance = nullptr;
                sc.InstantiateScript = nullptr;
                sc.DestroyScript = nullptr;
                current->hasScript = true;
                current->script = sc;
            }
        }
    }

    return !outEntities.empty();
}

bool PrefabLoader::SavePrefab(const Scene& scene, entt::entity root, const std::string& filePath) {
    const auto& reg = scene.Registry();
    if (!reg.valid(root)) return false;

    std::unordered_map<entt::entity, std::vector<entt::entity>> children;
    BuildChildrenMap(reg, children);

    std::vector<entt::entity> ordered;
    CollectSubtree(root, children, ordered);

    std::unordered_map<entt::entity, int> idMap;
    idMap.reserve(ordered.size());
    int nextId = 0;
    idMap[root] = nextId++;
    for (auto entity : ordered) {
        if (entity == root) continue;
        idMap[entity] = nextId++;
    }

    namespace fs = std::filesystem;
    try {
        fs::path outPath(filePath);
        if (outPath.has_parent_path()) {
            fs::create_directories(outPath.parent_path());
        }

        std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "PrefabLoader: Failed to open for write: " << filePath << std::endl;
            return false;
        }

        file << "# Prefab\n";
        file << std::fixed << std::setprecision(6);

        for (auto entity : ordered) {
            const int id = idMap[entity];
            file << "ENTITY " << id << "\n";

            if (reg.any_of<ParentComponent>(entity)) {
                auto parent = reg.get<ParentComponent>(entity).parent;
                if (parent != entt::null && idMap.find(parent) != idMap.end()) {
                    file << "PARENT " << idMap[parent] << "\n";
                }
            }

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
                    WriteModelOverrides(file, mc);
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
                file << "AUDIO " << path << " " << a.volume << " " << a.pitch << " "
                     << (a.loop ? 1 : 0) << " " << (a.playOnAwake ? 1 : 0) << " "
                     << (a.spatial ? 1 : 0) << " " << a.minDistance << " " << a.maxDistance << "\n";
            }

            if (reg.any_of<UIComponent>(entity)) {
                const auto& ui = reg.get<UIComponent>(entity);
                file << "UI " << (int)ui.type << " "
                     << ui.x << " " << ui.y << " " << ui.width << " " << ui.height << " "
                     << ui.color[0] << " " << ui.color[1] << " " << ui.color[2] << " " << ui.color[3] << " "
                     << (ui.useAnchors ? 1 : 0) << " " << ui.anchorX << " " << ui.anchorY << " " << ui.pivotX << " " << ui.pivotY << " "
                     << ui.backgroundColor[0] << " " << ui.backgroundColor[1] << " " << ui.backgroundColor[2] << " " << ui.backgroundColor[3] << " "
                     << (ui.drawBackground ? 1 : 0) << " "
                     << (ui.useTextAlign ? 1 : 0) << " " << (int)ui.textAlignH << " " << (int)ui.textAlignV << " "
                     << ui.textScale << " " << (ui.wrapText ? 1 : 0) << " " << ui.paddingX << " " << ui.paddingY << " "
                     << (ui.drawBorder ? 1 : 0) << " " << ui.borderThickness << " "
                     << ui.borderColor[0] << " " << ui.borderColor[1] << " " << ui.borderColor[2] << " " << ui.borderColor[3] << "\n";
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
                file << "PARTICLE " << p.duration << " " << (p.looping ? 1 : 0) << " " << (p.playOnAwake ? 1 : 0) << " "
                     << p.startLifetime << " " << p.startSpeed << " " << p.startSize << " "
                     << p.startColor[0] << " " << p.startColor[1] << " " << p.startColor[2] << " " << p.startColor[3] << " "
                     << p.rateOverTime << " " << p.emitterRadius << "\n";
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

            if (reg.any_of<ScriptComponent>(entity)) {
                const auto& sc = reg.get<ScriptComponent>(entity);
                if (!sc.className.empty()) {
                    file << "SCRIPT " << sc.className << "\n";
                }
            }

            file << "\n";
        }

        std::cout << "PrefabLoader: Saved prefab to " << filePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "PrefabLoader: SavePrefab failed: " << e.what() << std::endl;
        return false;
    }
}

entt::entity PrefabLoader::InstantiatePrefab(Scene& scene, const std::string& filePath, entt::entity parent) {
    std::vector<PrefabEntityData> entities;
    int rootId = -1;
    if (!ParsePrefabFile(filePath, entities, rootId)) {
        return entt::null;
    }

    auto& reg = scene.Registry();
    std::unordered_map<int, entt::entity> idToEntity;
    idToEntity.reserve(entities.size());

    for (const auto& data : entities) {
        entt::entity e = reg.create();
        idToEntity[data.id] = e;
    }

    entt::entity rootEntity = entt::null;
    for (const auto& data : entities) {
        entt::entity e = idToEntity[data.id];
        if (data.id == rootId) rootEntity = e;

        ApplyPrefabData(reg, e, data, false, false);

        PrefabLinkComponent link;
        link.prefabPath = filePath;
        link.prefabId = data.id;
        reg.emplace_or_replace<PrefabLinkComponent>(e, link);
    }

    for (const auto& data : entities) {
        if (data.parentId >= 0) {
            auto it = idToEntity.find(data.parentId);
            if (it != idToEntity.end()) {
                reg.emplace_or_replace<ParentComponent>(idToEntity[data.id], ParentComponent{it->second});
            }
        }
    }

    if (rootEntity == entt::null) {
        if (!entities.empty()) {
            rootEntity = idToEntity[entities.front().id];
        }
    }

    if (rootEntity != entt::null) {
        PrefabInstanceComponent instance;
        instance.prefabPath = filePath;
        instance.preserveRootTransform = true;
        reg.emplace_or_replace<PrefabInstanceComponent>(rootEntity, instance);

        if (parent != entt::null && reg.valid(parent)) {
            reg.emplace_or_replace<ParentComponent>(rootEntity, ParentComponent{parent});
        }
    }

    return rootEntity;
}

bool PrefabLoader::ApplyPrefab(Scene& scene, entt::entity root) {
    auto& reg = scene.Registry();
    if (!reg.valid(root) || !reg.any_of<PrefabInstanceComponent>(root)) return false;

    const auto& instance = reg.get<PrefabInstanceComponent>(root);
    if (instance.prefabPath.empty()) return false;

    std::vector<PrefabEntityData> prefabData;
    int rootId = -1;
    if (!ParsePrefabFile(instance.prefabPath, prefabData, rootId)) return false;

    std::unordered_map<entt::entity, std::vector<entt::entity>> children;
    BuildChildrenMap(reg, children);
    std::vector<entt::entity> subtree;
    CollectSubtree(root, children, subtree);

    std::unordered_map<int, entt::entity> prefabIdToEntity;
    prefabIdToEntity.reserve(subtree.size());
    for (auto entity : subtree) {
        if (reg.any_of<PrefabLinkComponent>(entity)) {
            const auto& link = reg.get<PrefabLinkComponent>(entity);
            if (link.prefabPath == instance.prefabPath && link.prefabId >= 0) {
                prefabIdToEntity[link.prefabId] = entity;
            }
        }
    }

    for (const auto& data : prefabData) {
        entt::entity target = entt::null;
        auto it = prefabIdToEntity.find(data.id);
        if (it != prefabIdToEntity.end()) {
            target = it->second;
        } else {
            target = reg.create();
            PrefabLinkComponent link;
            link.prefabPath = instance.prefabPath;
            link.prefabId = data.id;
            reg.emplace_or_replace<PrefabLinkComponent>(target, link);
            prefabIdToEntity[data.id] = target;
        }

        bool skipTransform = false;
        bool skipName = false;
        if (reg.any_of<PrefabLinkComponent>(target)) {
            const auto& link = reg.get<PrefabLinkComponent>(target);
            skipTransform = link.overrideTransform;
            skipName = link.overrideName;
        }
        if (target == root && instance.preserveRootTransform) {
            skipTransform = true;
        }

        ApplyPrefabData(reg, target, data, skipTransform, skipName);
    }

    for (const auto& data : prefabData) {
        if (data.id == rootId) continue;
        auto childIt = prefabIdToEntity.find(data.id);
        if (childIt == prefabIdToEntity.end()) continue;
        entt::entity child = childIt->second;

        if (data.parentId >= 0) {
            auto parentIt = prefabIdToEntity.find(data.parentId);
            if (parentIt != prefabIdToEntity.end()) {
                reg.emplace_or_replace<ParentComponent>(child, ParentComponent{parentIt->second});
            }
        }
    }

    return true;
}

} // namespace Genesis::Engine
