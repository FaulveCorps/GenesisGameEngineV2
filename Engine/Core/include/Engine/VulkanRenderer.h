#pragma once

#include "engine/IGraphics.h"
#include <memory>
#include <SDL.h>
#include <vector>
#include <string>
#include <unordered_map>

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
    MeshHandle CreateMesh(const MeshDesc& desc) override;
    void DestroyMesh(const MeshHandle& h) override;
    void DrawMesh(const MeshHandle& h) override;
    void DrawMesh(const MeshHandle& h, Material* material, const float* transform) override;
    void DrawTexture(Texture* tex, float x, float y, float w, float h,
                     float u0 = 0.f, float v0 = 0.f, float u1 = 1.f, float v1 = 1.f,
                     uint32_t color = 0xFFFFFFFF) override;
    void SetGlobalLight(const float direction[3], const float color[3], float intensity) override;
    void AddPointLight(const PointLightData& light) override;
    void ClearPointLights() override;
    void SetPostProcessParams(float exposure, float gamma) override;
    void SetPostProcessBloom(bool enabled) override;
    void SetPostProcessBloomThreshold(float threshold) override;
    void SetPostProcessVignette(bool enabled, float intensity, float radius, float softness) override;
    void SetPostProcessLUT(Texture* texture, bool enabled, float intensity) override;
    void SetShadowParams(float pcfRadius) override;
    void SetViewProjection(const float* view, const float* projection) override;
    // Deterministic helper used by tests and no-present mode to validate scene state ingestion.
    void ComputeStubClearColor(float outColor[4]) const;
    const float* GetLastComputedClearColor() const { return m_lastComputedClearColor; }
    void Shutdown() override;

    // Return true if the renderer has a usable swapchain and present capability
    bool IsPresentCapable() const;

    std::string GetName() const override { return std::string("vulkan"); }

private:
    SDL_Window* m_window = nullptr;

    struct VulkanMeshInfo {
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        bool hasNormals = false;
        bool hasUVs = false;
    };

    struct PendingMeshDraw {
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        bool hasNormals = false;
        bool hasUVs = false;
        bool hasMaterial = false;
        float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float metallic = 0.0f;
        float roughness = 1.0f;
        float transform[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };
    };

    struct PendingTextureDraw {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        uint32_t color = 0xFFFFFFFFu;
        bool hasTexture = false;
    };

    std::unordered_map<uint64_t, VulkanMeshInfo> m_meshes;
    std::vector<PendingMeshDraw> m_pendingMeshDraws;
    std::vector<PendingTextureDraw> m_pendingTextureDraws;
    uint64_t m_nextMeshId = 1;

    float m_view[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    float m_projection[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    float m_lightDir[3] = { 0.5f, 0.5f, 0.8f };
    float m_lightColor[3] = { 1.0f, 1.0f, 1.0f };
    float m_lightIntensity = 1.0f;
    std::vector<PointLightData> m_pointLights;

    float m_exposure = 1.0f;
    float m_gamma = 2.2f;
    bool m_bloomEnabled = true;
    float m_bloomThreshold = 1.0f;
    bool m_vignetteEnabled = false;
    float m_vignetteIntensity = 0.35f;
    float m_vignetteRadius = 0.75f;
    float m_vignetteSoftness = 0.25f;
    bool m_lutEnabled = false;
    float m_lutIntensity = 1.0f;
    Texture* m_lutTexture = nullptr;
    float m_shadowPcfRadius = 1.0f;

    uint32_t m_meshDrawCallsCurrent = 0;
    uint32_t m_meshDrawCallsLast = 0;
    uint32_t m_uiDrawCallsCurrent = 0;
    uint32_t m_uiDrawCallsLast = 0;
    float m_lastComputedClearColor[4] = { 0.1f, 0.6f, 0.2f, 1.0f };
    bool m_warnedMeshStub = false;

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
    std::vector<bool> m_swapchainImageInitialized;
    // Whether SDL successfully loaded the Vulkan loader (SDL_Vulkan_LoadLibrary)
    bool m_sdlVulkanLoaded = false;

    // Whether the VkSurfaceKHR was created via SDL_Vulkan_CreateSurface (requires SDL_WINDOW_VULKAN)
    bool m_surfaceCreatedViaSDL = false;

    // Optional host-side image used for shaderless triangle blit testing
    VkImage m_hostImage = VK_NULL_HANDLE;
    VkDeviceMemory m_hostImageMemory = VK_NULL_HANDLE;
    bool m_triangleEnabled = false;
#endif
    bool m_available = false;
};

} // namespace Genesis::Engine