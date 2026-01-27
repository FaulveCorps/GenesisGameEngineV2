#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <entt/entt.hpp>
#include "engine/Model.h"
#include "engine/Material.h"

namespace Genesis::Engine {

class ScriptableEntity;

struct ScriptComponent {
    ScriptableEntity* Instance = nullptr;

    // Script class name for editor/serialization
    std::string className;

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

// Stable, persistent identifier for deterministic scene serialization.
struct StableIdComponent {
    uint64_t id = 0;
};

// Hierarchy relationship (Editor + Runtime)
struct ParentComponent {
    entt::entity parent = entt::null;
};

// Prefab instance root marker
struct PrefabInstanceComponent {
    std::string prefabPath;
    bool preserveRootTransform = true;
};

// Prefab source link for entities created from prefabs
struct PrefabLinkComponent {
    std::string prefabPath;
    int prefabId = -1;
    bool overrideTransform = false;
    bool overrideName = false;
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

struct NavGridComponent {
    int width = 10;
    int height = 10;
    float cellSize = 1.0f;
    float originX = 0.0f;
    float originZ = 0.0f;
    float y = 0.0f;
    bool autoBakeColliders = true;
    bool drawDebug = true;
    bool debugPath = false;
    float debugStartX = 0.0f;
    float debugStartZ = 0.0f;
    float debugEndX = 1.0f;
    float debugEndZ = 1.0f;
};

struct NavAgentComponent {
    float speed = 2.0f;
    float targetX = 0.0f;
    float targetZ = 0.0f;
    bool hasTarget = false;
    float stopDistance = 0.1f;
    float repathInterval = 0.5f;
    bool drawPath = true;
};

struct NavPathPoint {
    int x = 0;
    int y = 0;
};

// Runtime-only state for navigation agents (not serialized)
struct NavAgentState {
    std::vector<NavPathPoint> path;
    size_t pathIndex = 0;
    float repathTimer = 0.0f;
};

} // namespace Genesis::Engine
