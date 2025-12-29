#pragma once

#include <array>
#include <string>

namespace Genesis::Engine {

struct Material {
    // Base color (RGBA)
    std::array<float,4> baseColor{1.0f, 1.0f, 1.0f, 1.0f};

    // Metallic/roughness PBR parameters
    float metallic = 0.0f;
    float roughness = 1.0f;

    // Optional texture references (paths or resource ids)
    std::string baseColorTexture;
    std::string normalTexture;

    Material() = default;
};

} // namespace Genesis::Engine
