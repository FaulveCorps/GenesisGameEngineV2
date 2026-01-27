#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/ScriptableEntity.h"
#include "engine/IGraphics.h"
#include "engine/IAudio.h"
#include "engine/MathUtils.h"
#include "engine/Animation.h"
#include "engine/UI.h"
#include "engine/DebugRenderer.h"
#include "engine/NavigationSystem.h"
#include "engine/Engine.h"
#include "engine/ScriptRegistry.h"
#include <iostream>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <unordered_set>

namespace Genesis::Engine {

static Matrix4 ComposeTransformMatrix(const Transform& t) {
    Matrix4 transMat = Matrix4::CreateTranslation(t.x, t.y, t.z);
    Matrix4 rotX = Matrix4::CreateRotationX(t.rx);
    Matrix4 rotY = Matrix4::CreateRotationY(t.ry);
    Matrix4 rotZ = Matrix4::CreateRotationZ(t.rz);
    Matrix4 scaleMat = Matrix4::CreateScale(t.sx, t.sy, t.sz);
    Matrix4 rot = rotZ * rotY * rotX;
    return transMat * rot * scaleMat;
}

void Scene::OnRuntimeStart() {
    m_runtimeActive = true;

    // Reset runtime-only state for audio/particles on each run.
    auto audioStateView = m_registry.view<AudioPlaybackState>();
    for (auto entity : audioStateView) {
        m_registry.remove<AudioPlaybackState>(entity);
    }

    auto particleStateView = m_registry.view<ParticleSystemState>();
    for (auto entity : particleStateView) {
        m_registry.remove<ParticleSystemState>(entity);
    }
}

void Scene::OnRuntimeStop() {
    m_runtimeActive = false;

    if (auto audio = GetAudioSubsystem()) {
        audio->StopAll();
    }

    auto view = m_registry.view<ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<ScriptComponent>(entity);
        if (sc.Instance) {
            sc.Instance->OnDestroy();
            if (sc.DestroyScript) sc.DestroyScript(&sc);
        }
    }

    auto audioStateView = m_registry.view<AudioPlaybackState>();
    for (auto entity : audioStateView) {
        m_registry.remove<AudioPlaybackState>(entity);
    }

    auto particleStateView = m_registry.view<ParticleSystemState>();
    for (auto entity : particleStateView) {
        m_registry.remove<ParticleSystemState>(entity);
    }
}

void Scene::OnUpdateRuntime(double dt) {
    m_runtimeActive = true;

    // Scripts
    {
        auto view = m_registry.view<ScriptComponent>();
        for (auto entity : view) {
            auto& sc = view.get<ScriptComponent>(entity);
            if (!sc.Instance) {
                if (!sc.InstantiateScript && !sc.className.empty()) {
                    if (const auto* info = ScriptRegistry::Find(sc.className)) {
                        sc.InstantiateScript = info->create;
                        sc.DestroyScript = info->destroy;
                    }
                }
                if (sc.InstantiateScript) {
                    sc.Instance = sc.InstantiateScript();
                    sc.Instance->m_Entity = entity;
                    sc.Instance->m_Scene = this;
                    sc.Instance->OnCreate();
                }
            }

            if (sc.Instance) {
                sc.Instance->OnUpdate(dt);
            }
        }
    }

    AnimationSystem::Update(*this, dt);
    UISystem::Update(*this, dt);
    NavigationSystem::UpdateAgents(*this, dt);

    if (auto audio = GetAudioSubsystem()) {
        audio->Update(dt);

        AudioListener listener;
        bool listenerSet = false;
        if (m_useSceneCamera) {
            auto camView = m_registry.view<CameraComponent, Transform>();
            entt::entity chosen = entt::null;
            for (auto entity : camView) {
                const auto& cam = camView.get<CameraComponent>(entity);
                if (cam.primary) {
                    chosen = entity;
                    break;
                }
                if (chosen == entt::null) {
                    chosen = entity;
                }
            }

            if (chosen != entt::null) {
                Matrix4 world = GetWorldMatrix(chosen);

                auto Normalize = [](float& x, float& y, float& z) {
                    float len = std::sqrt(x * x + y * y + z * z);
                    if (len > 1e-6f) { x /= len; y /= len; z /= len; }
                };

                float fwdX = world.m[8], fwdY = world.m[9], fwdZ = world.m[10];
                float upX = world.m[4], upY = world.m[5], upZ = world.m[6];
                Normalize(fwdX, fwdY, fwdZ);
                Normalize(upX, upY, upZ);

                listener.position[0] = world.m[12];
                listener.position[1] = world.m[13];
                listener.position[2] = world.m[14];
                listener.forward[0] = -fwdX;
                listener.forward[1] = -fwdY;
                listener.forward[2] = -fwdZ;
                listener.up[0] = upX;
                listener.up[1] = upY;
                listener.up[2] = upZ;
                listenerSet = true;
            }
        }

        if (listenerSet) {
            audio->SetListener(listener);
        }

        auto audioView = m_registry.view<AudioComponent>();
        for (auto entity : audioView) {
            auto& ac = audioView.get<AudioComponent>(entity);
            auto* state = m_registry.try_get<AudioPlaybackState>(entity);
            if (!state) {
                state = &m_registry.emplace<AudioPlaybackState>(entity);
            }
            if (ac.playOnAwake && !state->started && !ac.soundPath.empty()) {
                AudioPlayParams params;
                params.volume = ac.volume;
                params.pitch = ac.pitch;
                params.loop = ac.loop;
                params.spatial = ac.spatial;
                params.minDistance = ac.minDistance;
                params.maxDistance = ac.maxDistance;
                if (params.spatial) {
                    Matrix4 world = GetWorldMatrix(entity);
                    params.position[0] = world.m[12];
                    params.position[1] = world.m[13];
                    params.position[2] = world.m[14];
                }
                audio->PlayOneShot(ac.soundPath, params);
                state->started = true;
            }
        }
    }

    auto particleView = m_registry.view<ParticleSystemComponent>();
    for (auto entity : particleView) {
        auto& pc = particleView.get<ParticleSystemComponent>(entity);
        auto* state = m_registry.try_get<ParticleSystemState>(entity);
        if (!state) {
            state = &m_registry.emplace<ParticleSystemState>(entity);
        }

        if (pc.playOnAwake && !state->started) {
            state->started = true;
            state->playing = true;
            state->time = 0.0f;
        }

        if (state->playing) {
            state->time += static_cast<float>(dt);
            if (pc.duration > 0.0f && state->time >= pc.duration) {
                if (pc.looping) {
                    state->time = std::fmod(state->time, pc.duration);
                } else {
                    state->playing = false;
                }
            }
        }
    }

    // Demonstration: Rotate entities named "Cube" to visible show Play Mode is working
    auto view = m_registry.view<Transform, NameComponent>();
    for (auto entity : view) {
        auto& t = view.get<Transform>(entity);
        const auto& n = view.get<NameComponent>(entity);
        if (n.name == "Cube") {
            t.ry += (float)dt; 
            t.rx += (float)dt * 0.5f;
        }
    }
}

void Scene::OnUpdateEditor(double dt) {
    if (auto audio = GetAudioSubsystem()) {
        audio->Update(dt);
    }

    AnimationSystem::Update(*this, dt, true);

    auto particleView = m_registry.view<ParticleSystemComponent>();
    for (auto entity : particleView) {
        auto* state = m_registry.try_get<ParticleSystemState>(entity);
        if (!state || !state->playing) continue;

        const auto& pc = particleView.get<ParticleSystemComponent>(entity);
        state->time += static_cast<float>(dt);
        if (pc.duration > 0.0f && state->time >= pc.duration) {
            if (pc.looping) {
                state->time = std::fmod(state->time, pc.duration);
            } else {
                state->playing = false;
            }
        }
    }
}

Matrix4 Scene::GetWorldMatrix(entt::entity entity) const {
    if (!m_registry.valid(entity) || !m_registry.any_of<Transform>(entity)) {
        return Matrix4::CreateIdentity();
    }

    Matrix4 world = ComposeTransformMatrix(m_registry.get<Transform>(entity));

    entt::entity current = entity;
    std::unordered_set<entt::entity> visited;
    visited.insert(entity);
    int depth = 0;

    while (m_registry.any_of<ParentComponent>(current)) {
        auto parent = m_registry.get<ParentComponent>(current).parent;
        if (parent == entt::null || !m_registry.valid(parent)) break;
        if (visited.count(parent) > 0) break;
        visited.insert(parent);

        if (m_registry.any_of<Transform>(parent)) {
            Matrix4 parentLocal = ComposeTransformMatrix(m_registry.get<Transform>(parent));
            world = parentLocal * world;
        }

        current = parent;
        if (++depth > 64) break;
    }

    return world;
}

void Scene::Update(double dt) {
    OnUpdateRuntime(dt);
}

void Scene::CopyFrom(const Scene& other) {
    m_registry.clear();
    m_runtimeActive = false;
    m_useSceneCamera = other.m_useSceneCamera;

    std::vector<entt::entity> entities;
    other.m_registry.each([&](auto entity) { entities.push_back(entity); });

    std::unordered_map<entt::entity, entt::entity> remap;
    remap.reserve(entities.size());
    for (auto entity : entities) {
        remap[entity] = m_registry.create();
    }

    for (auto entity : entities) {
        const auto dst = remap[entity];

        if (auto* nc = other.m_registry.try_get<NameComponent>(entity)) {
            m_registry.emplace<NameComponent>(dst, *nc);
        }
        if (auto* sid = other.m_registry.try_get<StableIdComponent>(entity)) {
            m_registry.emplace<StableIdComponent>(dst, *sid);
        }
        if (auto* tc = other.m_registry.try_get<Transform>(entity)) {
            m_registry.emplace<Transform>(dst, *tc);
        }
        if (auto* pi = other.m_registry.try_get<PrefabInstanceComponent>(entity)) {
            m_registry.emplace<PrefabInstanceComponent>(dst, *pi);
        }
        if (auto* pl = other.m_registry.try_get<PrefabLinkComponent>(entity)) {
            m_registry.emplace<PrefabLinkComponent>(dst, *pl);
        }
        if (auto* lc = other.m_registry.try_get<LightComponent>(entity)) {
            m_registry.emplace<LightComponent>(dst, *lc);
        }
        if (auto* mc = other.m_registry.try_get<ModelComponent>(entity)) {
            m_registry.emplace<ModelComponent>(dst, *mc);
        }
        if (auto* cc = other.m_registry.try_get<CameraComponent>(entity)) {
            m_registry.emplace<CameraComponent>(dst, *cc);
        }
        if (auto* ac = other.m_registry.try_get<AudioComponent>(entity)) {
            m_registry.emplace<AudioComponent>(dst, *ac);
        }
        if (auto* pc = other.m_registry.try_get<ParticleSystemComponent>(entity)) {
            m_registry.emplace<ParticleSystemComponent>(dst, *pc);
        }
        if (auto* rc = other.m_registry.try_get<RigidBodyComponent>(entity)) {
            m_registry.emplace<RigidBodyComponent>(dst, *rc);
        }
        if (auto* bc = other.m_registry.try_get<BoxColliderComponent>(entity)) {
            m_registry.emplace<BoxColliderComponent>(dst, *bc);
        }
        if (auto* sc = other.m_registry.try_get<SphereColliderComponent>(entity)) {
            m_registry.emplace<SphereColliderComponent>(dst, *sc);
        }
        if (auto* ng = other.m_registry.try_get<NavGridComponent>(entity)) {
            m_registry.emplace<NavGridComponent>(dst, *ng);
        }
        if (auto* na = other.m_registry.try_get<NavAgentComponent>(entity)) {
            m_registry.emplace<NavAgentComponent>(dst, *na);
        }
        if (auto* scp = other.m_registry.try_get<ScriptComponent>(entity)) {
            ScriptComponent copy = *scp;
            copy.Instance = nullptr;
            m_registry.emplace<ScriptComponent>(dst, copy);
        }
        if (auto* ui = other.m_registry.try_get<UIComponent>(entity)) {
            m_registry.emplace<UIComponent>(dst, *ui);
        }
        if (auto* anim = other.m_registry.try_get<AnimationComponent>(entity)) {
            m_registry.emplace<AnimationComponent>(dst, *anim);
        }
    }

    // Apply parent relationships after all entities exist.
    for (auto entity : entities) {
        if (auto* pc = other.m_registry.try_get<ParentComponent>(entity)) {
            entt::entity parent = pc->parent;
            entt::entity mappedParent = entt::null;
            if (parent != entt::null) {
                auto it = remap.find(parent);
                if (it != remap.end()) mappedParent = it->second;
            }
            if (mappedParent != entt::null) {
                m_registry.emplace<ParentComponent>(remap[entity], ParentComponent{mappedParent});
            }
        }
    }
}

void Scene::Render(IGraphicsAPI* renderer) {
    if (!renderer) return;

    std::unordered_map<entt::entity, Matrix4> worldCache;
    std::unordered_set<entt::entity> visiting;

    auto ComputeWorld = [&](auto&& self, entt::entity e) -> Matrix4 {
        if (!m_registry.valid(e)) return Matrix4::CreateIdentity();
        if (auto it = worldCache.find(e); it != worldCache.end()) return it->second;

        Matrix4 local = Matrix4::CreateIdentity();
        if (m_registry.any_of<Transform>(e)) {
            local = ComposeTransformMatrix(m_registry.get<Transform>(e));
        }

        if (visiting.count(e) > 0) {
            return local;
        }
        visiting.insert(e);

        Matrix4 world = local;
        if (m_registry.any_of<ParentComponent>(e)) {
            auto parent = m_registry.get<ParentComponent>(e).parent;
            if (parent != entt::null && m_registry.valid(parent)) {
                world = self(self, parent) * local;
            }
        }

        visiting.erase(e);
        worldCache[e] = world;
        return world;
    };

    if (m_runtimeActive && m_useSceneCamera) {
        auto camView = m_registry.view<CameraComponent, Transform>();
        entt::entity chosen = entt::null;
        for (auto entity : camView) {
            const auto& cam = camView.get<CameraComponent>(entity);
            if (cam.primary) {
                chosen = entity;
                break;
            }
            if (chosen == entt::null) {
                chosen = entity;
            }
        }

        if (chosen != entt::null) {
            const auto& cam = camView.get<CameraComponent>(chosen);
            Matrix4 world = ComputeWorld(ComputeWorld, chosen);

            float px = world.m[12];
            float py = world.m[13];
            float pz = world.m[14];

            auto Normalize = [](float& x, float& y, float& z) {
                float len = std::sqrt(x * x + y * y + z * z);
                if (len > 1e-6f) { x /= len; y /= len; z /= len; }
            };

            float rightX = world.m[0], rightY = world.m[1], rightZ = world.m[2];
            float upX = world.m[4], upY = world.m[5], upZ = world.m[6];
            float fwdX = world.m[8], fwdY = world.m[9], fwdZ = world.m[10];
            Normalize(rightX, rightY, rightZ);
            Normalize(upX, upY, upZ);
            Normalize(fwdX, fwdY, fwdZ);

            Matrix4 view = Matrix4::CreateIdentity();
            view.m[0] = rightX; view.m[1] = upX; view.m[2] = -fwdX; view.m[3] = 0.0f;
            view.m[4] = rightY; view.m[5] = upY; view.m[6] = -fwdY; view.m[7] = 0.0f;
            view.m[8] = rightZ; view.m[9] = upZ; view.m[10] = -fwdZ; view.m[11] = 0.0f;
            view.m[12] = -(rightX * px + rightY * py + rightZ * pz);
            view.m[13] = -(upX * px + upY * py + upZ * pz);
            view.m[14] = (fwdX * px + fwdY * py + fwdZ * pz);
            view.m[15] = 1.0f;

            const float aspect = 16.0f / 9.0f;
            float nearPlane = (cam.nearPlane < 0.01f) ? 0.01f : cam.nearPlane;
            float farPlane = (cam.farPlane <= nearPlane) ? (nearPlane + 0.01f) : cam.farPlane;
            const float fovRad = cam.fov * 0.01745329252f;
            Matrix4 projection = Matrix4::CreatePerspective(fovRad, aspect, nearPlane, farPlane);

            renderer->SetViewProjection(view.m, projection.m);
        } else {
            Matrix4 view = Matrix4::CreateIdentity();
            Matrix4 projection = Matrix4::CreateIdentity();
            renderer->SetViewProjection(view.m, projection.m);
        }
    }

    // Clear old point lights
    renderer->ClearPointLights();

    // 1. Setup Lights
    auto lightView = m_registry.view<LightComponent, Transform>();
    bool lightFound = false;
    for (auto entity : lightView) {
        auto& light = lightView.get<LightComponent>(entity);
        Matrix4 world = ComputeWorld(ComputeWorld, entity);
        
        if (light.type == LightType::Directional) {
            if (!lightFound) {
                float dir[3] = { world.m[8], world.m[9], world.m[10] };
                float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
                if (len > 1e-6f) { dir[0] /= len; dir[1] /= len; dir[2] /= len; }
                
                renderer->SetGlobalLight(dir, light.color, light.intensity);
                lightFound = true;
            }
        } else if (light.type == LightType::Point) {
            IGraphicsAPI::PointLightData pl;
            pl.position[0] = world.m[12]; pl.position[1] = world.m[13]; pl.position[2] = world.m[14];
            pl.color[0] = light.color[0]; pl.color[1] = light.color[1]; pl.color[2] = light.color[2];
            pl.intensity = light.intensity;
            pl.radius = light.range;
            renderer->AddPointLight(pl);
        }
    }

    if (!lightFound) {
        // Set a default light if none in scene
        float defaultDir[3] = {0.5f, 0.5f, 0.8f};
        float defaultColor[3] = {1.0f, 1.0f, 1.0f};
        renderer->SetGlobalLight(defaultDir, defaultColor, 1.0f);
    }

    // 2. Render Models
    auto view = m_registry.view<ModelComponent>();
    for (auto entity : view) {
        auto &mc = view.get<ModelComponent>(entity);
        if (mc.model) {
            Matrix4 transform = ComputeWorld(ComputeWorld, entity);
            mc.model->Draw(transform.m, &mc.materialOverrides);
        }
    }

    // Debug Renderer (Colliders)
    DebugRenderer::Render(*this, renderer);

    // 3. Render UI
    UISystem::Render(*this, renderer);
}

} // namespace Genesis::Engine
