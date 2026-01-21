#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/ScriptableEntity.h"
#include "engine/IGraphics.h"
#include "engine/IAudio.h"
#include "engine/MathUtils.h"
#include "engine/Animation.h"
#include "engine/UI.h"
#include "engine/DebugRenderer.h"
#include "engine/Engine.h"
#include <iostream>
#include <cmath>

namespace Genesis::Engine {

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

    if (auto audio = GetAudioSubsystem()) {
        audio->Update(dt);
        auto audioView = m_registry.view<AudioComponent>();
        for (auto entity : audioView) {
            auto& ac = audioView.get<AudioComponent>(entity);
            auto* state = m_registry.try_get<AudioPlaybackState>(entity);
            if (!state) {
                state = &m_registry.emplace<AudioPlaybackState>(entity);
            }
            if (ac.playOnAwake && !state->started && !ac.soundPath.empty()) {
                audio->PlayOneShot(ac.soundPath, ac.volume);
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
}

void Scene::Update(double dt) {
    OnUpdateRuntime(dt);
}

void Scene::CopyFrom(const Scene& other) {
    m_registry.clear();
    m_runtimeActive = false;
    m_useSceneCamera = other.m_useSceneCamera;

    other.m_registry.each([&](auto entity) {
        const auto dst = m_registry.create();

        if (auto* nc = other.m_registry.try_get<NameComponent>(entity)) {
            m_registry.emplace<NameComponent>(dst, *nc);
        }
        if (auto* tc = other.m_registry.try_get<Transform>(entity)) {
            m_registry.emplace<Transform>(dst, *tc);
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
    });
}

void Scene::Render(IGraphicsAPI* renderer) {
    if (!renderer) return;

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
            const auto& t = camView.get<Transform>(chosen);

            Matrix4 rotX = Matrix4::CreateRotationX(-t.rx);
            Matrix4 rotY = Matrix4::CreateRotationY(-t.ry);
            Matrix4 rotZ = Matrix4::CreateRotationZ(-t.rz);
            Matrix4 trans = Matrix4::CreateTranslation(-t.x, -t.y, -t.z);
            Matrix4 view = rotX * rotY * rotZ * trans;

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
        auto& t = lightView.get<Transform>(entity);
        
        if (light.type == LightType::Directional) {
            if (!lightFound) {
                Matrix4 rotX = Matrix4::CreateRotationX(t.rx);
                Matrix4 rotY = Matrix4::CreateRotationY(t.ry);
                Matrix4 rotZ = Matrix4::CreateRotationZ(t.rz);
                Matrix4 rot = rotZ * rotY * rotX; 

                float dir[3] = { rot.m[8], rot.m[9], rot.m[10] }; 
                
                renderer->SetGlobalLight(dir, light.color, light.intensity);
                lightFound = true;
            }
        } else if (light.type == LightType::Point) {
            IGraphicsAPI::PointLightData pl;
            pl.position[0] = t.x; pl.position[1] = t.y; pl.position[2] = t.z;
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
            Matrix4 transform = Matrix4::CreateIdentity();
            
            if (m_registry.any_of<Transform>(entity)) {
                auto& t = m_registry.get<Transform>(entity);
                
                Matrix4 transMat = Matrix4::CreateTranslation(t.x, t.y, t.z);
                Matrix4 rotX = Matrix4::CreateRotationX(t.rx);
                Matrix4 rotY = Matrix4::CreateRotationY(t.ry);
                Matrix4 rotZ = Matrix4::CreateRotationZ(t.rz);
                Matrix4 scaleMat = Matrix4::CreateScale(t.sx, t.sy, t.sz);
                
                // T * R * S
                Matrix4 rot = rotZ * rotY * rotX;
                transform = transMat * rot * scaleMat;
            }

            mc.model->Draw(transform.m, &mc.materialOverrides);
        }
    }

    // Debug Renderer (Colliders)
    DebugRenderer::Render(*this, renderer);

    // 3. Render UI
    UISystem::Render(*this, renderer);
}

} // namespace Genesis::Engine
