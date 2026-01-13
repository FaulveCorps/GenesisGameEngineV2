#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/IGraphics.h"
#include "engine/MathUtils.h"
#include "engine/Animation.h"
#include "engine/UI.h"
#include <iostream>

namespace Genesis::Engine {

void Scene::OnRuntimeStart() {
}

void Scene::OnRuntimeStop() {
}

void Scene::OnUpdateRuntime(double dt) {
    AnimationSystem::Update(*this, dt);
    UISystem::Update(*this, dt);

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
    // Not implemented yet
}

void Scene::Render(IGraphicsAPI* renderer) {
    if (!renderer) return;

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

            mc.model->Draw(transform.m);
        }
    }

    // 3. Render UI
    UISystem::Render(*this, renderer);
}

} // namespace Genesis::Engine
