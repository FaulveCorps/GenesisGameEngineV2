#pragma once

#include "engine/IGraphics.h"
#include <memory>
#include <SDL.h>
#include <vector>
#include <string>

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

    // Return true if the renderer has a usable swapchain and present capability
    bool IsPresentCapable() const;

    std::string GetName() const override { return std::string("vulkan"); }

private:
    SDL_Window* m_window = nullptr;
#ifdef HAVE_VULKAN
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = UINT32_MAX;
    uint32_t m_presentQueueFamily = UINT32_MAX;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent{};
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_inFlightFence = VK_NULL_HANDLE;
    bool m_sdlVulkan = false;

    // Optional host-side image used for shaderless triangle blit testing
    VkImage m_hostImage = VK_NULL_HANDLE;
    VkDeviceMemory m_hostImageMemory = VK_NULL_HANDLE;
    bool m_triangleEnabled = false;
#endif
    bool m_available = false;
};

} // namespace Genesis::Engine