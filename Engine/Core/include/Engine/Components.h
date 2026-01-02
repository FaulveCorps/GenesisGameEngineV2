#pragma once

#include <memory>
#include "engine/Model.h"

namespace Genesis::Engine {

struct Transform {
    float x = 0.f, y = 0.f, z = 0.f;
    float rx = 0.f, ry = 0.f, rz = 0.f;
    float sx = 1.f, sy = 1.f, sz = 1.f;
};

struct ModelComponent {
    std::shared_ptr<Model> model;
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

} // namespace Genesis::Engine
