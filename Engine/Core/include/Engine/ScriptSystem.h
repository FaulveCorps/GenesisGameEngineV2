#pragma once

#include "engine/Scene.h"

namespace Genesis::Engine {

class ScriptSystem {
public:
    static void Update(Scene& scene, double dt, bool simulate = true);
    static void OnDestroy(Scene& scene, entt::entity entity);
};

} // namespace Genesis::Engine
