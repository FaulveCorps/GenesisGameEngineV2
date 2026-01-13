#pragma once

#include <memory>
#include <string>
#include "engine/Model.h"

namespace Genesis::Engine {

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

struct ModelComponent {
    std::shared_ptr<Model> model;

    // Optional source asset path used for scene save/reload.
    // (If empty, the model cannot be serialized by SceneLoader::SaveScene.)
    std::string sourcePath;
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

} // namespace Genesis::Engine
