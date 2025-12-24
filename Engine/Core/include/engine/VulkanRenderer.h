#pragma once

#include "engine/IGraphics.h"
#include <memory>
#include <SDL.h>

#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace Genesis::Engine {

class VulkanRenderer : public IGraphicsAPI {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

private:
    SDL_Window* m_window = nullptr;
#ifdef HAVE_VULKAN
    VkInstance m_instance = VK_NULL_HANDLE;
#endif
    bool m_available = false;
};

} // namespace Genesis::Engine