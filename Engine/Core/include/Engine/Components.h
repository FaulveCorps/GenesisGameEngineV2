#pragma once

#include <memory>
#include <string>
#include <map>
#include <entt/entt.hpp>
#include "engine/Model.h"
#include "engine/Material.h"

namespace Genesis::Engine {

class ScriptableEntity;

struct ScriptComponent {
    ScriptableEntity* Instance = nullptr;

    ScriptableEntity* (*InstantiateScript)() = nullptr;
    void (*DestroyScript)(ScriptComponent*) = nullptr;

    template<typename T>
    void Bind() {
        InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
        DestroyScript = [](ScriptComponent* sc) { delete sc->Instance; sc->Instance = nullptr; };
    }
};

struct Transform {
    float x = 0.f, y = 0.f, z = 0.f;
    // Rotation is stored in radians (matches Engine math utilities).
    float rx = 0.f, ry = 0.f, rz = 0.f;
    float sx = 1.f, sy = 1.f, sz = 1.f;
};

// Optional human-readable entity name (Editor / tooling).
struct NameComponent {
    std::string name;
};

// Hierarchy relationship (Editor + Runtime)
struct ParentComponent {
    entt::entity parent = entt::null;
};

struct ModelComponent {
    std::shared_ptr<Model> model;

    // Optional source asset path used for scene save/reload.
    // (If empty, the model cannot be serialized by SceneLoader::SaveScene.)
    std::string sourcePath;

    // Material overrides tailored for this instance.
    // Key: material index in the model.
    std::map<int, Material> materialOverrides;
};

enum class LightType {
    Directional,
    Point
};

struct LightComponent {
    LightType type = LightType::Directional;
    float color[3] = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    // For point lights
    float range = 10.0f; 
};

struct CameraComponent {
    float fov = 45.0f; // degrees
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool primary = true;
};

struct AudioComponent {
    std::string soundPath;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool playOnAwake = true;
    
    // 3D Spatial Settings
    bool spatial = true;
    float minDistance = 1.0f;
    float maxDistance = 20.0f;
};

// Runtime-only state for AudioComponent (not serialized)
struct AudioPlaybackState {
    bool started = false;
};

struct ParticleSystemComponent {
    float duration = 5.0f;
    bool looping = true;
    bool playOnAwake = true;
    
    float startLifetime = 5.0f;
    float startSpeed = 5.0f;
    float startSize = 1.0f;
    float startColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    
    // Emission
    float rateOverTime = 10.0f;
    
    // Shape (Cone, Sphere, Box)
    // For now simplistic
    float emitterRadius = 0.5f;
};

// Runtime-only state for ParticleSystemComponent (not serialized)
struct ParticleSystemState {
    float time = 0.0f;
    bool playing = false;
    bool started = false;
};

struct BoxColliderComponent {
    float size[3] = {1.0f, 1.0f, 1.0f};
    float offset[3] = {0.0f, 0.0f, 0.0f};
    bool isTrigger = false;
};

struct SphereColliderComponent {
    float radius = 0.5f;
    float offset[3] = {0.0f, 0.0f, 0.0f};
    bool isTrigger = false;
};

struct RigidBodyComponent {
    float mass = 1.0f;
    bool useGravity = true;
    bool isKinematic = false;
};

} // namespace Genesis::Engine
