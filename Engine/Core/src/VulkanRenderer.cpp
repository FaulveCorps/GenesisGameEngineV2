#include "engine/VulkanRenderer.h"
#include <SDL_vulkan.h>
#include <iostream>
#include <vector>

namespace Genesis::Engine {

bool VulkanRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
    m_window = window;

    // Try to load Vulkan loader via SDL
    if (SDL_Vulkan_LoadLibrary(nullptr) != 0) {
        std::cerr << "VulkanRenderer: SDL_Vulkan_LoadLibrary failed: " << SDL_GetError() << std::endl;
        m_available = false;
        return false;
    }

    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr)) {
        std::cerr << "VulkanRenderer: SDL_Vulkan_GetInstanceExtensions failed" << std::endl;
        m_available = false;
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, extensions.data())) {
        std::cerr << "VulkanRenderer: SDL_Vulkan_GetInstanceExtensions failed (2)" << std::endl;
        m_available = false;
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::cout << "VulkanRenderer: SDL reported " << count << " required instance extensions:\n";
    for (unsigned int i = 0; i < count; ++i) std::cout << "  " << extensions[i] << std::endl;

#ifdef HAVE_VULKAN
    // Minimal VkInstance create (no validation layers by default)
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GenesisGameEngine Vulkan Smoke Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.pEngineName = "Genesis";
    appInfo.engineVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = count;
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult r = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (r != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkCreateInstance failed: " << r << std::endl;
        m_available = false;
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::cout << "VulkanRenderer: instance created" << std::endl;
    m_available = true;
    return true;
#else
    std::cout << "VulkanRenderer: compiled without Vulkan support (HAVE_VULKAN not defined)" << std::endl;
    m_available = false;
    SDL_Vulkan_UnloadLibrary();
    return false;
#endif
}

void VulkanRenderer::BeginFrame() {
    if (!m_available) return;
    // nothing yet: this is a smoke test stub
}

void VulkanRenderer::EndFrame() {
    if (!m_available) return;
    // nothing yet
}

void VulkanRenderer::Shutdown() {
#ifdef HAVE_VULKAN
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
#endif
    SDL_Vulkan_UnloadLibrary();
    m_window = nullptr;
    m_available = false;
}

} // namespace Genesis::Engine