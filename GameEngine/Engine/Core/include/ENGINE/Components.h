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

} // namespace Genesis::Engine
