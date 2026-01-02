#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/IGraphics.h"
#include "engine/MathUtils.h"
#include <iostream>

namespace Genesis::Engine {

void Scene::Update(double /*dt*/) {
    // placeholder for systems (physics, animation, etc.)
}

void Scene::Render(IGraphicsAPI* renderer) {
    if (!renderer) return;

    // 1. Setup Global Light
    // Find the first entity with a LightComponent
    auto lightView = m_registry.view<LightComponent, Transform>();
    bool lightFound = false;
    for (auto entity : lightView) {
        auto& light = lightView.get<LightComponent>(entity);
        auto& t = lightView.get<Transform>(entity);
        
        // Calculate direction from rotation (assuming default forward is -Z or similar)
        // For simplicity, let's just use the rotation as Euler angles to create a forward vector
        // Or just use the position if it was a point light, but for Directional we need direction.
        // Let's assume Transform rotation is in radians.
        // A simple forward vector from Euler angles (pitch, yaw, roll)
        // But our Transform struct has rx, ry, rz.
        
        // Let's construct a rotation matrix and extract forward vector.
        Matrix4 rotX = Matrix4::CreateRotationX(t.rx);
        Matrix4 rotY = Matrix4::CreateRotationY(t.ry);
        Matrix4 rotZ = Matrix4::CreateRotationZ(t.rz);
        Matrix4 rot = rotZ * rotY * rotX; // ZYX order is common

        // Assuming default light direction is (0, 0, -1) or similar.
        // In OpenGL, camera looks down -Z. Light coming from +Z is standard "front" light.
        // Let's assume the light direction vector points FROM the light source.
        // Or TO the light source?
        // pbr.frag uses: vec3 lightDir = normalize(uLightDir); float NdotL = max(dot(n, lightDir), 0.0);
        // Usually N dot L implies L is vector TO light.
        // So if light is at (0,10,0), L is (0,1,0).
        
        // Let's use the Z axis of the rotation matrix as the direction.
        // If identity, Z is (0,0,1).
        float dir[3] = { rot.m[8], rot.m[9], rot.m[10] }; // 3rd column (Z axis)
        
        // If we want "To Light", and the object is rotated to look at the scene...
        // Let's just pass the values and tweak in game.
        
        renderer->SetGlobalLight(dir, light.color, light.intensity);
        lightFound = true;
        break; // Only support one global light for now
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
}

} // namespace Genesis::Engine
