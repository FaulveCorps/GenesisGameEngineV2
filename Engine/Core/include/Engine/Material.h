#pragma once

#include <array>
#include <string>
#include <memory>
#include "Engine/Texture.h"

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

    // Loaded texture objects
    std::shared_ptr<Texture> baseColorTextureObj;
    std::shared_ptr<Texture> normalTextureObj;

    Material() = default;
};

} // namespace Genesis::Engine
