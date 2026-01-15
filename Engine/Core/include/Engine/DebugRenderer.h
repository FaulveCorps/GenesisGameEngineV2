#pragma once
#include "engine/Scene.h"
#include "engine/IGraphics.h"

namespace Genesis::Engine {

class DebugRenderer {
public:
    static void Render(Scene& scene, IGraphicsAPI* renderer);
};

} // namespace Genesis::Engine
