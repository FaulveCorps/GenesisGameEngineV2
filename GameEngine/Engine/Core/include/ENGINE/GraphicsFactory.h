#pragma once

#include "engine/IGraphics.h"
#include <memory>
#include <SDL.h>
#include <string>
#include <vector>

namespace Genesis::Engine {

class GraphicsFactory {
public:
    // Try to create and initialize a renderer using the requested priority. Returns nullptr on failure.
    // Valid names (case-insensitive): "vulkan", "directx", "opengl"
    // requirePresentForVulkan: if true, Vulkan will only be considered successful if it supports swapchain/present
    static std::unique_ptr<IGraphicsAPI> CreateRenderer(SDL_Window* window, SDL_GLContext glContext, const std::vector<std::string>& priorityOrder, bool requirePresentForVulkan = false);
};

} // namespace Genesis::Engine
