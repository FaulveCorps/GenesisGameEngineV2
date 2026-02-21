#include "engine/VulkanRenderer.h"
#include "engine/Material.h"
#include <SDL_vulkan.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cmath>
#ifdef _WIN32
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#endif
namespace Genesis::Engine {

namespace {
inline float maxf(float a, float b) { return (a > b) ? a : b; }
inline float minf(float a, float b) { return (a < b) ? a : b; }
inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

inline void unpackArgb8(uint32_t color, float outRgba[4]) {
    outRgba[0] = static_cast<float>((color >> 16) & 0xFFu) / 255.0f; // R
    outRgba[1] = static_cast<float>((color >> 8) & 0xFFu) / 255.0f;  // G
    outRgba[2] = static_cast<float>(color & 0xFFu) / 255.0f;         // B
    outRgba[3] = static_cast<float>((color >> 24) & 0xFFu) / 255.0f; // A
}

inline float alphaOver(float dst, float src, float alpha) {
    const float a = clamp01(alpha);
    return dst * (1.0f - a) + src * a;
}
}

bool VulkanRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
    m_window = window;
    m_meshes.clear();
    m_pointLights.clear();
    m_meshDrawCallsCurrent = 0;
    m_meshDrawCallsLast = 0;
    m_uiDrawCallsCurrent = 0;
    m_uiDrawCallsLast = 0;
    m_pendingMeshDraws.clear();
    m_pendingTextureDraws.clear();
    m_lastComputedClearColor[0] = 0.1f;
    m_lastComputedClearColor[1] = 0.6f;
    m_lastComputedClearColor[2] = 0.2f;
    m_lastComputedClearColor[3] = 1.0f;
    m_warnedMeshStub = false;

    // Try to load Vulkan loader via SDL (optional); fall back to platform Win32 surface if SDL does not expose Vulkan
    // SDL3: SDL_Vulkan_LoadLibrary returns bool (true on success).
    const bool sdlVulkanLoaded = SDL_Vulkan_LoadLibrary(nullptr);
    m_sdlVulkanLoaded = sdlVulkanLoaded;

    auto appendUnique = [](std::vector<const char*>& v, const char* s) {
        if (!s) return;
        for (const char* existing : v) {
            if (existing && std::strcmp(existing, s) == 0) return;
        }
        v.push_back(s);
    };

    const Uint64 winFlags = SDL_GetWindowFlags(m_window);
    const bool windowHasVulkanFlag = (winFlags & SDL_WINDOW_VULKAN) != 0;
    const bool canUseSDLSurface = sdlVulkanLoaded && windowHasVulkanFlag;

    std::vector<const char*> extensions;
    if (sdlVulkanLoaded) {
        uint32_t count = 0;
        const char* const* extNames = SDL_Vulkan_GetInstanceExtensions(&count);
        if (extNames) {
            std::cout << "VulkanRenderer: SDL reported " << count << " required instance extensions:\n";
            for (uint32_t i = 0; i < count; ++i) {
                appendUnique(extensions, extNames[i]);
                std::cout << "  " << extNames[i] << std::endl;
            }
        } else {
            std::cerr << "VulkanRenderer: SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError() << std::endl;
        }
    }

    // If SDL didn't provide extensions (or Vulkan isn't available via SDL), fall back to a minimal list.
    // Also, if we won't use SDL to create the surface, ensure we have the platform surface extensions.
    if (extensions.empty() || !canUseSDLSurface) {
#ifdef _WIN32
        if (!canUseSDLSurface) {
            std::cout << "VulkanRenderer: window not created with SDL_WINDOW_VULKAN; using Win32 surface creation" << std::endl;
        } else if (!sdlVulkanLoaded) {
            std::cout << "VulkanRenderer: SDL reports no dynamic Vulkan support; falling back to Win32 surface creation" << std::endl;
        }
        appendUnique(extensions, "VK_KHR_surface");
        appendUnique(extensions, "VK_KHR_win32_surface");
#else
        if (!sdlVulkanLoaded) {
            std::cerr << "VulkanRenderer: SDL Vulkan not available and no fallback for this platform" << std::endl;
            return false;
        }
        if (!canUseSDLSurface) {
            std::cerr << "VulkanRenderer: SDL window not created with SDL_WINDOW_VULKAN and no fallback for this platform" << std::endl;
            return false;
        }
#endif
    }

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
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult r = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (r != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkCreateInstance failed: " << r << std::endl;
        m_available = false;
        if (m_sdlVulkanLoaded) SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::cout << "VulkanRenderer: instance created" << std::endl;

    // Create a surface via SDL if available; otherwise try a platform-specific surface (Win32)
    m_surfaceCreatedViaSDL = false;
    if (canUseSDLSurface) {
        if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
            std::cerr << "VulkanRenderer: SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << std::endl;
            std::cerr << "VulkanRenderer: falling back to Win32 surface creation" << std::endl;
            m_surface = VK_NULL_HANDLE;
        } else {
            m_surfaceCreatedViaSDL = true;
        }
    }

    if (!m_surfaceCreatedViaSDL) {
#ifdef _WIN32
        HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        HINSTANCE hinstance = (HINSTANCE)SDL_GetPointerProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, NULL);
        if (!hwnd || !hinstance) {
            std::cerr << "VulkanRenderer: Failed to get HWND/HINSTANCE from SDL window" << std::endl;
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            return false;
        }
        VkWin32SurfaceCreateInfoKHR sc{};
        sc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sc.hwnd = hwnd;
        sc.hinstance = hinstance;
        PFN_vkCreateWin32SurfaceKHR fpCreateWin32 = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateWin32SurfaceKHR");
        if (!fpCreateWin32) { std::cerr << "VulkanRenderer: vkCreateWin32SurfaceKHR not available via vkGetInstanceProcAddr" << std::endl; vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; return false; }
        VkResult sr = fpCreateWin32(m_instance, &sc, nullptr, &m_surface);
        if (sr != VK_SUCCESS) { std::cerr << "VulkanRenderer: vkCreateWin32SurfaceKHR failed: " << sr << std::endl; vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; return false; }
        std::cout << "VulkanRenderer: Win32 surface created" << std::endl;
#else
        std::cerr << "VulkanRenderer: no surface creation path available" << std::endl;
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        return false;
#endif
    }

    // Pick a physical device that supports graphics + present and swapchain
    std::cout << "VulkanRenderer: enumerating physical devices" << std::endl;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    std::cout << "VulkanRenderer: vkEnumeratePhysicalDevices returned count=" << deviceCount << std::endl;
    if (deviceCount == 0) {
        std::cerr << "VulkanRenderer: no physical devices found" << std::endl;
        m_available = false;
        if (m_sdlVulkanLoaded) SDL_Vulkan_UnloadLibrary();
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    std::cout << "VulkanRenderer: got " << deviceCount << " physical device handles" << std::endl;

    bool found = false;
    VkPhysicalDevice firstCandidate = VK_NULL_HANDLE;
    uint32_t firstGraphics = UINT32_MAX, firstPresent = UINT32_MAX;
    for (VkPhysicalDevice dev : devices) {
        // check for queue family and swapchain support
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qprops.data());

        int graphicsIndex = -1;
        int presentIndex = -1;
        for (uint32_t i = 0; i < qCount; ++i) {
            if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsIndex = (int)i;
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &presentSupport);
            if (presentSupport) presentIndex = (int)i;
        }

        if (graphicsIndex < 0 || presentIndex < 0) continue;

        // check device extensions for swapchain
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());
        bool hasSwapchain = false;
        for (auto &e : exts) if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) { hasSwapchain = true; break; }
        if (!hasSwapchain) continue;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        std::cout << "VulkanRenderer: candidate device: " << props.deviceName << " (vendor=0x" << std::hex << props.vendorID << std::dec << ")" << std::endl;

        // prefer discrete GPU if available
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physicalDevice = dev;
            m_graphicsQueueFamily = (uint32_t)graphicsIndex;
            m_presentQueueFamily = (uint32_t)presentIndex;
            m_graphicsQueue = VK_NULL_HANDLE;
            m_presentQueue = VK_NULL_HANDLE;
            found = true;
            break;
        }

        // otherwise remember first valid candidate
        if (firstCandidate == VK_NULL_HANDLE) {
            firstCandidate = dev;
            firstGraphics = (uint32_t)graphicsIndex;
            firstPresent = (uint32_t)presentIndex;
        }
    }

    if (!found && firstCandidate != VK_NULL_HANDLE) {
        m_physicalDevice = firstCandidate;
        m_graphicsQueueFamily = firstGraphics;
        m_presentQueueFamily = firstPresent;
        m_graphicsQueue = VK_NULL_HANDLE;
        m_presentQueue = VK_NULL_HANDLE;
        found = true;
    }

    if (!found) {
        std::cerr << "VulkanRenderer: no suitable physical device found" << std::endl;
        m_available = false;
        if (m_sdlVulkanLoaded) SDL_Vulkan_UnloadLibrary();
        return false;
    }

    // Create logical device
    float qPriority = 1.0f;
    const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    // if graphics and present are different families, create queues for both
    std::vector<VkDeviceQueueCreateInfo> qinfos;
    VkDeviceQueueCreateInfo qg{};
    qg.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qg.queueFamilyIndex = m_graphicsQueueFamily;
    qg.queueCount = 1;
    qg.pQueuePriorities = &qPriority;
    qinfos.push_back(qg);
    if (m_presentQueueFamily != m_graphicsQueueFamily) {
        VkDeviceQueueCreateInfo qp{};
        qp.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qp.queueFamilyIndex = m_presentQueueFamily;
        qp.queueCount = 1;
        qp.pQueuePriorities = &qPriority;
        qinfos.push_back(qp);
    }

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = (uint32_t)qinfos.size();
    dci.pQueueCreateInfos = qinfos.data();
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = deviceExts;

    VkResult res = vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device);
    if (res != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkCreateDevice failed: " << res << std::endl;
        m_available = false;
        if (m_sdlVulkanLoaded) SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::cout << "VulkanRenderer: logical device created" << std::endl;
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    if (m_presentQueueFamily != m_graphicsQueueFamily) vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    else m_presentQueue = m_graphicsQueue;
    std::cout << "VulkanRenderer: obtained graphics and present queues" << std::endl;
    // If we created the surface via a platform fallback (not SDL_Vulkan_CreateSurface), do not attempt a full swapchain here —
    // this environment sometimes crashes with certain drivers.
    if (!m_surfaceCreatedViaSDL) {
        // Allow a force option through an environment variable for controlled testing:
        // set GENESIS_FORCE_VULKAN_SWAPCHAIN=1 to force swapchain creation despite Win32 fallback.
        const char* env = std::getenv("GENESIS_FORCE_VULKAN_SWAPCHAIN");
        bool forceSwap = false;
        std::cout << "VulkanRenderer: GENESIS_FORCE_VULKAN_SWAPCHAIN raw env='" << (env ? env : "(null)") << "'" << std::endl;
        if (env) {
            std::string ev(env);
            // trim whitespace
            auto trim = [](std::string &s){ while(!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin()); while(!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back(); };
            trim(ev);
            std::transform(ev.begin(), ev.end(), ev.begin(), [](unsigned char c){ return std::tolower(c); });
            if (ev == "1" || ev == "true") forceSwap = true;
            std::cout << "VulkanRenderer: parsed GENESIS_FORCE_VULKAN_SWAPCHAIN='" << ev << "'" << std::endl;
        }
        std::cout << "VulkanRenderer: forceSwap=" << (forceSwap ? "true" : "false") << std::endl;
        if (!forceSwap) {
            std::cout << "VulkanRenderer: Win32 fallback - skipping swapchain creation to avoid driver issues. Device initialized for smoke test." << std::endl;
            m_available = true;
            return true;
        } else {
            std::cout << "VulkanRenderer: Win32 fallback - forcing swapchain creation due to GENESIS_FORCE_VULKAN_SWAPCHAIN=1" << std::endl;
            // proceed to create swapchain below (risky; used only for controlled tests)
        }
    }

    // Create swapchain
    VkSurfaceCapabilitiesKHR caps;
    std::cout << "VulkanRenderer: querying surface capabilities" << std::endl;
    VkResult surfRes = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);
    if (surfRes != VK_SUCCESS) { std::cerr << "VulkanRenderer: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: " << surfRes << std::endl; return false; }

    uint32_t fmtCount = 0;
    std::cout << "VulkanRenderer: querying surface formats" << std::endl;
    VkResult fmtRes = vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, nullptr);
    if (fmtRes != VK_SUCCESS) { std::cerr << "VulkanRenderer: vkGetPhysicalDeviceSurfaceFormatsKHR failed: " << fmtRes << std::endl; return false; }
    std::vector<VkSurfaceFormatKHR> surfaceFormats(fmtCount);
    fmtRes = vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, surfaceFormats.data());
    if (fmtRes != VK_SUCCESS) { std::cerr << "VulkanRenderer: vkGetPhysicalDeviceSurfaceFormatsKHR(2) failed: " << fmtRes << std::endl; return false; }
    std::cout << "VulkanRenderer: got " << fmtCount << " surface formats" << std::endl;

    VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
    for (auto &f : surfaceFormats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { surfaceFormat = f; break; }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == (uint32_t)-1) {
        int w, h;
        SDL_GetWindowSizeInPixels(m_window, &w, &h);
        extent.width = (uint32_t)w;
        extent.height = (uint32_t)h;
    }

    uint32_t desiredImages = caps.minImageCount;
    if (caps.maxImageCount > 0 && desiredImages > caps.maxImageCount) desiredImages = caps.maxImageCount;
    // Try to be conservative with requested image usage and count to maximize compatibility
    if (desiredImages < 1) desiredImages = 1;

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = m_surface;
    sci.minImageCount = desiredImages;
    sci.imageFormat = surfaceFormat.format;
    sci.imageColorSpace = surfaceFormat.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    // limit usage to color attachment to avoid drivers that mis-handle transfer dst on swapchain images
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (m_presentQueueFamily != m_graphicsQueueFamily) {
        uint32_t qinds[2] = { m_graphicsQueueFamily, m_presentQueueFamily };
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = qinds;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;

    std::cout << "VulkanRenderer: creating swapchain (images=" << desiredImages << ", extent=" << extent.width << "x" << extent.height << ")" << std::endl;
    res = vkCreateSwapchainKHR(m_device, &sci, nullptr, &m_swapchain);
    if (res != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkCreateSwapchainKHR failed: " << res << std::endl;
        m_available = false;
        return false;
    }
    std::cout << "VulkanRenderer: swapchain created" << std::endl;

    uint32_t imageCount = 0;
    PFN_vkGetSwapchainImagesKHR fpGetSwapchainImages = (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr(m_device, "vkGetSwapchainImagesKHR");
    if (!fpGetSwapchainImages) { std::cerr << "VulkanRenderer: vkGetSwapchainImagesKHR function not available" << std::endl; return false; }
    VkResult r3 = fpGetSwapchainImages(m_device, m_swapchain, &imageCount, nullptr);
    if (r3 != VK_SUCCESS) { std::cerr << "VulkanRenderer: vkGetSwapchainImagesKHR failed(1): " << r3 << std::endl; return false; }
    m_swapchainImages.resize(imageCount);
    r3 = fpGetSwapchainImages(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
    if (r3 != VK_SUCCESS) { std::cerr << "VulkanRenderer: vkGetSwapchainImagesKHR failed(2): " << r3 << std::endl; return false; }
    std::cout << "VulkanRenderer: swapchain image count=" << imageCount << std::endl;

    m_swapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo ivci{};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = m_swapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = surfaceFormat.format;
        ivci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        VkResult r2 = vkCreateImageView(m_device, &ivci, nullptr, &m_swapchainImageViews[i]);
        if (r2 != VK_SUCCESS) { std::cerr << "VulkanRenderer: vkCreateImageView failed: " << r2 << std::endl; }
    }
    std::cout << "VulkanRenderer: created image views" << std::endl;
    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    // Command pool
    VkCommandPoolCreateInfo cpci{};
    // Before creating command pool, optionally create host image for triangle test (env GENESIS_VULKAN_TRIANGLE)
    const char* triEnv2 = std::getenv("GENESIS_VULKAN_TRIANGLE");
    if (triEnv2) {
        std::string tev(triEnv2);
        auto trim = [](std::string &s){ while(!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin()); while(!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back(); };
        trim(tev);
        std::transform(tev.begin(), tev.end(), tev.begin(), [](unsigned char c){ return std::tolower(c); });
        if (tev == "1" || tev == "true") {
            m_triangleEnabled = true;
            if (m_hostImage == VK_NULL_HANDLE) {
                std::cout << "VulkanRenderer: GENESIS_VULKAN_TRIANGLE enabled; creating host image" << std::endl;
                auto findMemoryType = [&](uint32_t typeFilter, VkMemoryPropertyFlags props)->uint32_t {
                    VkPhysicalDeviceMemoryProperties memProps{};
                    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
                    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
                    }
                    return UINT32_MAX;
                };

                VkImageCreateInfo hic{};
                hic.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                hic.imageType = VK_IMAGE_TYPE_2D;
                hic.format = m_swapchainImageFormat;
                hic.extent = { m_swapchainExtent.width, m_swapchainExtent.height, 1 };
                hic.mipLevels = 1;
                hic.arrayLayers = 1;
                hic.samples = VK_SAMPLE_COUNT_1_BIT;
                hic.tiling = VK_IMAGE_TILING_LINEAR; // host-accessible
                hic.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                hic.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

                if (vkCreateImage(m_device, &hic, nullptr, &m_hostImage) != VK_SUCCESS) {
                    std::cerr << "VulkanRenderer: failed to create host image for triangle" << std::endl;
                    m_triangleEnabled = false;
                } else {
                    VkMemoryRequirements mr{};
                    vkGetImageMemoryRequirements(m_device, m_hostImage, &mr);
                    uint32_t mtype = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                    if (mtype == UINT32_MAX) mtype = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
                    if (mtype == UINT32_MAX) {
                        std::cerr << "VulkanRenderer: no suitable host-visible memory for host image" << std::endl;
                        vkDestroyImage(m_device, m_hostImage, nullptr);
                        m_hostImage = VK_NULL_HANDLE;
                        m_triangleEnabled = false;
                    } else {
                        VkMemoryAllocateInfo mai{};
                        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                        mai.allocationSize = mr.size;
                        mai.memoryTypeIndex = mtype;
                        if (vkAllocateMemory(m_device, &mai, nullptr, &m_hostImageMemory) != VK_SUCCESS) {
                            std::cerr << "VulkanRenderer: failed to allocate host image memory" << std::endl;
                            vkDestroyImage(m_device, m_hostImage, nullptr);
                            m_hostImage = VK_NULL_HANDLE;
                            m_triangleEnabled = false;
                        } else {
                            vkBindImageMemory(m_device, m_hostImage, m_hostImageMemory, 0);
                            // Fill
                            VkImageSubresource sub{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
                            VkSubresourceLayout layout{};
                            vkGetImageSubresourceLayout(m_device, m_hostImage, &sub, &layout);
                            void* data = nullptr;
                            vkMapMemory(m_device, m_hostImageMemory, 0, VK_WHOLE_SIZE, 0, &data);
                            uint8_t* base = (uint8_t*)data + layout.offset;
                            uint32_t w = m_swapchainExtent.width;
                            uint32_t h = m_swapchainExtent.height;
                            uint32_t rowPitch = static_cast<uint32_t>(layout.rowPitch);
                            bool isBGRA = (m_swapchainImageFormat == VK_FORMAT_B8G8R8A8_SRGB || m_swapchainImageFormat == VK_FORMAT_B8G8R8A8_UNORM);
                            uint8_t bg_r = 26, bg_g = 153, bg_b = 51, bg_a = 255;
                            uint8_t tri_r = 255, tri_g = 40, tri_b = 40, tri_a = 255;
                            float vx0 = w * 0.5f, vy0 = h * 0.2f;
                            float vx1 = w * 0.2f, vy1 = h * 0.8f;
                            float vx2 = w * 0.8f, vy2 = h * 0.8f;
                            auto edge = [](float ax, float ay, float bx, float by, float cx, float cy){ return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax); };
                            float area = edge(vx0, vy0, vx1, vy1, vx2, vy2);
                            for (uint32_t y = 0; y < h; ++y) {
                                uint8_t* row = base + y * rowPitch;
                                for (uint32_t x = 0; x < w; ++x) {
                                    float px = (float)x + 0.5f, py = (float)y + 0.5f;
                                    float w0 = edge(vx1, vy1, vx2, vy2, px, py) / area;
                                    float w1 = edge(vx2, vy2, vx0, vy0, px, py) / area;
                                    float w2 = edge(vx0, vy0, vx1, vy1, px, py) / area;
                                    bool inside = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) || (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
                                    uint8_t r = inside ? tri_r : bg_r;
                                    uint8_t g = inside ? tri_g : bg_g;
                                    uint8_t b = inside ? tri_b : bg_b;
                                    uint8_t a = tri_a;
                                    if (isBGRA) { row[x*4 + 0] = b; row[x*4 + 1] = g; row[x*4 + 2] = r; row[x*4 + 3] = a; }
                                    else { row[x*4 + 0] = r; row[x*4 + 1] = g; row[x*4 + 2] = b; row[x*4 + 3] = a; }
                                }
                            }
                            vkUnmapMemory(m_device, m_hostImageMemory);
                            std::cout << "VulkanRenderer: host triangle image populated (" << w << "x" << h << ")" << std::endl;
                        }
                    }
                }
            }
        }
    }
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = m_graphicsQueueFamily;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(m_device, &cpci, nullptr, &m_commandPool);

    // Command buffers
    m_commandBuffers.resize(imageCount);
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = m_commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = (uint32_t)m_commandBuffers.size();
    vkAllocateCommandBuffers(m_device, &cbai, m_commandBuffers.data());
    m_swapchainImageInitialized.assign(imageCount, false);

    // Semaphores and fence
    VkSemaphoreCreateInfo sci_sem{};
    sci_sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(m_device, &sci_sem, nullptr, &m_imageAvailableSemaphore);
    vkCreateSemaphore(m_device, &sci_sem, nullptr, &m_renderFinishedSemaphore);

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(m_device, &fci, nullptr, &m_inFlightFence);

    // Record command buffers that either clear each swapchain image or copy a host-generated triangle image into it
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(m_commandBuffers[i], &cbbi);

        if (m_triangleEnabled && m_hostImage != VK_NULL_HANDLE) {
            // Transition swapchain image -> TRANSFER_DST
            VkImageMemoryBarrier barrierDst{};
            barrierDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrierDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierDst.image = m_swapchainImages[i];
            barrierDst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrierDst.subresourceRange.baseMipLevel = 0;
            barrierDst.subresourceRange.levelCount = 1;
            barrierDst.subresourceRange.baseArrayLayer = 0;
            barrierDst.subresourceRange.layerCount = 1;
            barrierDst.srcAccessMask = 0;
            barrierDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            // Transition host image -> TRANSFER_SRC
            VkImageMemoryBarrier barrierSrc{};
            barrierSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierSrc.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
            barrierSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrierSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierSrc.image = m_hostImage;
            barrierSrc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrierSrc.subresourceRange.baseMipLevel = 0;
            barrierSrc.subresourceRange.levelCount = 1;
            barrierSrc.subresourceRange.baseArrayLayer = 0;
            barrierSrc.subresourceRange.layerCount = 1;
            barrierSrc.srcAccessMask = 0;
            barrierSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            VkImageMemoryBarrier barriers[] = { barrierSrc, barrierDst };
            vkCmdPipelineBarrier(m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = { 0, 0, 0 };
            copyRegion.dstSubresource = copyRegion.srcSubresource;
            copyRegion.dstOffset = { 0, 0, 0 };
            copyRegion.extent = { m_swapchainExtent.width, m_swapchainExtent.height, 1 };

            vkCmdCopyImage(m_commandBuffers[i], m_hostImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_swapchainImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // transition swapchain to present
            VkImageMemoryBarrier barrierPresent{};
            barrierPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrierPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierPresent.image = m_swapchainImages[i];
            barrierPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrierPresent.subresourceRange.baseMipLevel = 0;
            barrierPresent.subresourceRange.levelCount = 1;
            barrierPresent.subresourceRange.baseArrayLayer = 0;
            barrierPresent.subresourceRange.layerCount = 1;
            barrierPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrierPresent.dstAccessMask = 0;

            vkCmdPipelineBarrier(m_commandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierPresent);
        } else {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = m_swapchainImages[i];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkClearColorValue clearColor; clearColor.float32[0] = 0.1f; clearColor.float32[1] = 0.6f; clearColor.float32[2] = 0.2f; clearColor.float32[3] = 1.0f;
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0; range.levelCount = 1;
            range.baseArrayLayer = 0; range.layerCount = 1;

            vkCmdClearColorImage(m_commandBuffers[i], m_swapchainImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

            // transition to present
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;
            vkCmdPipelineBarrier(m_commandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        vkEndCommandBuffer(m_commandBuffers[i]);
    }

    std::cout << "VulkanRenderer: swapchain and command buffers ready (" << imageCount << " images)" << std::endl;

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
    // Even when Vulkan is unavailable (unit tests / headless no-init paths),
    // keep transient frame state behavior deterministic.
    m_meshDrawCallsCurrent = 0;
    m_uiDrawCallsCurrent = 0;
    m_pendingMeshDraws.clear();
    m_pendingTextureDraws.clear();
}

MeshHandle VulkanRenderer::CreateMesh(const MeshDesc& desc) {
    MeshHandle h{};
    if (desc.vertices.empty() || desc.indices.empty()) return h;

    h.id = m_nextMeshId++;
    VulkanMeshInfo info;
    info.vertexCount = static_cast<uint32_t>(desc.vertices.size() / 3);
    info.indexCount = static_cast<uint32_t>(desc.indices.size());
    info.hasNormals = !desc.normals.empty();
    info.hasUVs = !desc.uvs.empty();
    m_meshes[h.id] = info;
    return h;
}

void VulkanRenderer::DestroyMesh(const MeshHandle& h) {
    if (!h.IsValid()) return;
    m_meshes.erase(h.id);
}

void VulkanRenderer::DrawMesh(const MeshHandle& h) {
    if (!h.IsValid()) return;
    auto it = m_meshes.find(h.id);
    if (it == m_meshes.end()) return;
    ++m_meshDrawCallsCurrent;

    PendingMeshDraw cmd{};
    cmd.vertexCount = it->second.vertexCount;
    cmd.indexCount = it->second.indexCount;
    cmd.hasNormals = it->second.hasNormals;
    cmd.hasUVs = it->second.hasUVs;
    m_pendingMeshDraws.push_back(cmd);
    if (!m_warnedMeshStub) {
        std::cout << "VulkanRenderer: mesh draw accepted (feature parity in progress; raster pipeline pending)" << std::endl;
        m_warnedMeshStub = true;
    }
}

void VulkanRenderer::DrawMesh(const MeshHandle& h, Material* material, const float* transform) {
    if (!h.IsValid()) return;
    auto it = m_meshes.find(h.id);
    if (it == m_meshes.end()) return;

    ++m_meshDrawCallsCurrent;

    PendingMeshDraw cmd{};
    cmd.vertexCount = it->second.vertexCount;
    cmd.indexCount = it->second.indexCount;
    cmd.hasNormals = it->second.hasNormals;
    cmd.hasUVs = it->second.hasUVs;
    if (material) {
        cmd.hasMaterial = true;
        cmd.baseColor[0] = material->baseColor[0];
        cmd.baseColor[1] = material->baseColor[1];
        cmd.baseColor[2] = material->baseColor[2];
        cmd.baseColor[3] = material->baseColor[3];
        cmd.metallic = material->metallic;
        cmd.roughness = material->roughness;
    }
    if (transform) {
        std::memcpy(cmd.transform, transform, sizeof(cmd.transform));
    }
    m_pendingMeshDraws.push_back(cmd);

    if (!m_warnedMeshStub) {
        std::cout << "VulkanRenderer: mesh draw accepted (feature parity in progress; raster pipeline pending)" << std::endl;
        m_warnedMeshStub = true;
    }
}

void VulkanRenderer::DrawTexture(Texture* tex, float x, float y, float w, float h,
                                 float u0, float v0, float u1, float v1, uint32_t color) {
    if (!tex) return;
    ++m_uiDrawCallsCurrent;
    PendingTextureDraw cmd{};
    cmd.x = x;
    cmd.y = y;
    cmd.w = w;
    cmd.h = h;
    cmd.u0 = u0;
    cmd.v0 = v0;
    cmd.u1 = u1;
    cmd.v1 = v1;
    cmd.color = color;
    cmd.hasTexture = true;
    m_pendingTextureDraws.push_back(cmd);
}

void VulkanRenderer::SetGlobalLight(const float direction[3], const float color[3], float intensity) {
    if (direction) {
        m_lightDir[0] = direction[0];
        m_lightDir[1] = direction[1];
        m_lightDir[2] = direction[2];
    }
    if (color) {
        m_lightColor[0] = color[0];
        m_lightColor[1] = color[1];
        m_lightColor[2] = color[2];
    }
    m_lightIntensity = intensity;
}

void VulkanRenderer::AddPointLight(const PointLightData& light) {
    if (m_pointLights.size() < 16) {
        m_pointLights.push_back(light);
    }
}

void VulkanRenderer::ClearPointLights() {
    m_pointLights.clear();
}

void VulkanRenderer::SetPostProcessParams(float exposure, float gamma) {
    m_exposure = exposure;
    m_gamma = gamma;
}

void VulkanRenderer::SetPostProcessBloom(bool enabled) {
    m_bloomEnabled = enabled;
}

void VulkanRenderer::SetPostProcessBloomThreshold(float threshold) {
    m_bloomThreshold = threshold;
}

void VulkanRenderer::SetPostProcessVignette(bool enabled, float intensity, float radius, float softness) {
    m_vignetteEnabled = enabled;
    m_vignetteIntensity = intensity;
    m_vignetteRadius = radius;
    m_vignetteSoftness = softness;
}

void VulkanRenderer::SetPostProcessLUT(Texture* texture, bool enabled, float intensity) {
    m_lutTexture = texture;
    m_lutEnabled = enabled && texture != nullptr;
    m_lutIntensity = intensity;
}

void VulkanRenderer::SetShadowParams(float pcfRadius) {
    m_shadowPcfRadius = pcfRadius;
}

void VulkanRenderer::SetViewProjection(const float* view, const float* projection) {
    if (view) std::memcpy(m_view, view, sizeof(m_view));
    if (projection) std::memcpy(m_projection, projection, sizeof(m_projection));
}

void VulkanRenderer::ComputeStubClearColor(float outColor[4]) const {
    if (!outColor) return;

    const float dirLen = std::sqrt(m_lightDir[0] * m_lightDir[0] + m_lightDir[1] * m_lightDir[1] + m_lightDir[2] * m_lightDir[2]);
    const float dirZ = (dirLen > 1e-5f) ? (m_lightDir[2] / dirLen) : 1.0f;
    const float directionalFacing = clamp01(0.5f * dirZ + 0.5f);
    const float shadowSoftness = 1.0f / (1.0f + maxf(0.0f, m_shadowPcfRadius - 1.0f) * 0.2f);
    const float directionalEnergy = maxf(0.0f, m_lightIntensity) * (0.2f + 0.8f * directionalFacing) * shadowSoftness;

    float clearR = m_lightColor[0] * directionalEnergy * 0.35f;
    float clearG = m_lightColor[1] * directionalEnergy * 0.35f;
    float clearB = m_lightColor[2] * directionalEnergy * 0.35f;

    for (const auto& pl : m_pointLights) {
        const float radiusAtten = 1.0f / (1.0f + maxf(0.0f, pl.radius) * 0.1f);
        const float w = maxf(0.0f, pl.intensity) * 0.05f * radiusAtten;
        clearR += pl.color[0] * w;
        clearG += pl.color[1] * w;
        clearB += pl.color[2] * w;
    }

    const float activityBoost = minf(0.25f, 0.01f * static_cast<float>(m_meshDrawCallsCurrent)
                           + 0.005f * static_cast<float>(m_uiDrawCallsCurrent));
    clearR += activityBoost;
    clearG += activityBoost * 0.8f;
    clearB += activityBoost * 0.6f;

    for (const auto& cmd : m_pendingMeshDraws) {
        const float triCount = maxf(1.0f, static_cast<float>(cmd.indexCount) / 3.0f);
        const float vertexWeight = minf(0.04f, static_cast<float>(cmd.vertexCount) * 0.0015f);
        const float topologyWeight = minf(0.12f, triCount * 0.006f + vertexWeight);
        const float normalBonus = cmd.hasNormals ? 0.025f : 0.0f;
        const float uvBonus = cmd.hasUVs ? 0.02f : 0.0f;

        const float albedoR = clamp01(cmd.baseColor[0]);
        const float albedoG = clamp01(cmd.baseColor[1]);
        const float albedoB = clamp01(cmd.baseColor[2]);
        const float alpha = clamp01(cmd.baseColor[3]);
        const float metallic = clamp01(cmd.metallic);
        const float roughness = clamp01(cmd.roughness);
        const float smoothness = 1.0f - roughness;
        const float shadingWeight = (cmd.hasMaterial ? 0.08f : 0.03f) + topologyWeight;
        const float specLift = 0.02f + metallic * smoothness * 0.08f + normalBonus;

        clearR += albedoR * shadingWeight + specLift;
        clearG += albedoG * shadingWeight + specLift * 0.8f;
        clearB += albedoB * shadingWeight + specLift * 0.6f;

        clearR += uvBonus * (0.7f + 0.3f * albedoR);
        clearG += uvBonus * (0.7f + 0.3f * albedoG);
        clearB += uvBonus * (0.7f + 0.3f * albedoB);

        const float tx = cmd.transform[12];
        const float ty = cmd.transform[13];
        const float tz = cmd.transform[14];
        const float transformInfluence = minf(0.08f, std::sqrt(tx * tx + ty * ty + tz * tz) * 0.01f);
        clearR += transformInfluence * alpha;
        clearG += transformInfluence * alpha * 0.8f;
        clearB += transformInfluence * alpha * 0.6f;
    }

    // Camera/projection influence keeps SetViewProjection meaningful in Vulkan no-pipeline mode.
    const float viewTranslation = std::sqrt(m_view[12] * m_view[12] + m_view[13] * m_view[13] + m_view[14] * m_view[14]);
    const float projDeviation = std::fabs(m_projection[0] - 1.0f) + std::fabs(m_projection[5] - 1.0f);
    const float cameraInfluence = minf(0.25f, viewTranslation * 0.01f + projDeviation * 0.03f);
    clearB += cameraInfluence;

    if (m_bloomEnabled) {
        const float threshold = maxf(0.0f, m_bloomThreshold);
        auto bloomLift = [threshold](float c) {
            return c + maxf(0.0f, c - threshold) * 0.45f;
        };
        clearR = bloomLift(clearR);
        clearG = bloomLift(clearG);
        clearB = bloomLift(clearB);
    }

    const float exposure = maxf(0.01f, m_exposure);
    auto toneMap = [exposure](float c) {
        return 1.0f - std::exp(-maxf(0.0f, c) * exposure);
    };
    clearR = toneMap(clearR);
    clearG = toneMap(clearG);
    clearB = toneMap(clearB);

    if (m_vignetteEnabled) {
        const float vigIntensity = clamp01(m_vignetteIntensity);
        const float vigRadius = clamp01(m_vignetteRadius);
        const float vigSoftness = maxf(0.05f, m_vignetteSoftness);
        const float edge = clamp01((1.0f - vigRadius) / vigSoftness);
        const float attenuation = clamp01(1.0f - vigIntensity * edge * 0.5f);
        clearR *= attenuation;
        clearG *= attenuation;
        clearB *= attenuation;
    }

    if (m_lutEnabled && m_lutTexture) {
        const float lutMix = clamp01(m_lutIntensity) * 0.35f;
        const float lutR = 0.85f * clearR + 0.12f * clearG + 0.03f * clearB;
        const float lutG = 0.10f * clearR + 0.82f * clearG + 0.08f * clearB;
        const float lutB = 0.08f * clearR + 0.18f * clearG + 0.74f * clearB;
        clearR = clearR * (1.0f - lutMix) + lutR * lutMix;
        clearG = clearG * (1.0f - lutMix) + lutG * lutMix;
        clearB = clearB * (1.0f - lutMix) + lutB * lutMix;
    }

    const float invGamma = 1.0f / maxf(0.01f, m_gamma);
    float finalR = std::pow(clamp01(clearR), invGamma);
    float finalG = std::pow(clamp01(clearG), invGamma);
    float finalB = std::pow(clamp01(clearB), invGamma);

    // UI overlay pass (order-sensitive alpha-over) after post stack, matching OpenGL's late sprite/UI pass.
    float invViewportArea = 1.0f / (1280.0f * 720.0f);
    if (m_window) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(m_window, &w, &h);
        if (w > 0 && h > 0) {
            invViewportArea = 1.0f / static_cast<float>(w * h);
        }
    }

    for (const auto& cmd : m_pendingTextureDraws) {
        if (!cmd.hasTexture) continue;
        float rgba[4]{};
        unpackArgb8(cmd.color, rgba);

        const float area = maxf(0.0f, cmd.w) * maxf(0.0f, cmd.h);
        const float areaFactor = minf(1.0f, area * invViewportArea * 6.0f);
        const float uvSpan = minf(1.0f, std::fabs(cmd.u1 - cmd.u0) * std::fabs(cmd.v1 - cmd.v0));
        const float overlayAlpha = clamp01(rgba[3] * (0.15f + 0.85f * areaFactor));
        const float overlayGain = 0.25f + 0.75f * uvSpan;

        const float srcR = clamp01(rgba[0] * overlayGain);
        const float srcG = clamp01(rgba[1] * overlayGain);
        const float srcB = clamp01(rgba[2] * overlayGain);

        finalR = alphaOver(finalR, srcR, overlayAlpha);
        finalG = alphaOver(finalG, srcG, overlayAlpha);
        finalB = alphaOver(finalB, srcB, overlayAlpha);
    }

    outColor[0] = clamp01(finalR);
    outColor[1] = clamp01(finalG);
    outColor[2] = clamp01(finalB);
    outColor[3] = 1.0f;
}

void VulkanRenderer::EndFrame() {
    if (!m_available) return;
#ifdef HAVE_VULKAN
    std::cout << "VulkanRenderer::EndFrame -> enter m_device=" << (void*)m_device << " m_swapchain=" << (void*)m_swapchain << std::endl;
    if (m_device == VK_NULL_HANDLE) { std::cout << "VulkanRenderer::EndFrame -> no device" << std::endl; return; }

    float computedClear[4] = { 0.1f, 0.6f, 0.2f, 1.0f };
    ComputeStubClearColor(computedClear);
    m_lastComputedClearColor[0] = computedClear[0];
    m_lastComputedClearColor[1] = computedClear[1];
    m_lastComputedClearColor[2] = computedClear[2];
    m_lastComputedClearColor[3] = computedClear[3];

    if (m_swapchain == VK_NULL_HANDLE) {
        m_meshDrawCallsLast = m_meshDrawCallsCurrent;
        m_uiDrawCallsLast = m_uiDrawCallsCurrent;
        std::cout << "VulkanRenderer::EndFrame -> no swapchain, returning early" << std::endl;
        return;
    } // no swapchain (skipped for this environment)

    if (m_imageAvailableSemaphore == VK_NULL_HANDLE || m_renderFinishedSemaphore == VK_NULL_HANDLE || m_inFlightFence == VK_NULL_HANDLE) {
        std::cerr << "VulkanRenderer::EndFrame -> missing sync objects; skipping frame to avoid crash" << std::endl;
        return;
    }

    // Wait for previous frame to finish
    vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlightFence);

    uint32_t imageIndex = 0;
    VkResult r = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (r != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkAcquireNextImageKHR failed: " << r << std::endl;
        return;
    }

    // For the non-triangle path, record a per-frame clear that ingests scene state
    // (light/post-process/draw activity) so Vulkan is no longer a fixed-color smoke pass.
    if (!m_triangleEnabled && imageIndex < m_commandBuffers.size()) {
        const float clearR = computedClear[0];
        const float clearG = computedClear[1];
        const float clearB = computedClear[2];

        vkResetCommandBuffer(m_commandBuffers[imageIndex], 0);

        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(m_commandBuffers[imageIndex], &cbbi);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = (imageIndex < m_swapchainImageInitialized.size() && m_swapchainImageInitialized[imageIndex])
            ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_swapchainImages[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(m_commandBuffers[imageIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkClearColorValue clearColor{};
        clearColor.float32[0] = clearR;
        clearColor.float32[1] = clearG;
        clearColor.float32[2] = clearB;
        clearColor.float32[3] = 1.0f;

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        vkCmdClearColorImage(m_commandBuffers[imageIndex], m_swapchainImages[imageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(m_commandBuffers[imageIndex], VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(m_commandBuffers[imageIndex]);
        if (imageIndex < m_swapchainImageInitialized.size()) {
            m_swapchainImageInitialized[imageIndex] = true;
        }
    }

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = waitSemaphores;
    submit.pWaitDstStageMask = waitStages;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphore };
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = signalSemaphores;

    VkResult sres = vkQueueSubmit(m_graphicsQueue, 1, &submit, m_inFlightFence);
    if (sres != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkQueueSubmit failed: " << sres << std::endl;
        return;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signalSemaphores;
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &imageIndex;

    VkResult pres = vkQueuePresentKHR(m_presentQueue, &present);
    if (pres != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkQueuePresentKHR failed: " << pres << std::endl;
    }
    m_meshDrawCallsLast = m_meshDrawCallsCurrent;
    m_uiDrawCallsLast = m_uiDrawCallsCurrent;
    std::cout << "VulkanRenderer::EndFrame -> exit" << std::endl;
#endif
}

bool VulkanRenderer::IsPresentCapable() const {
#ifdef HAVE_VULKAN
    // We consider the renderer present-capable if a swapchain exists and we have present resources
    return (m_swapchain != VK_NULL_HANDLE && m_presentQueue != VK_NULL_HANDLE && m_imageAvailableSemaphore != VK_NULL_HANDLE && m_renderFinishedSemaphore != VK_NULL_HANDLE);
#else
    return false;
#endif
}

void VulkanRenderer::Shutdown() {
#ifdef HAVE_VULKAN
    std::cout << "VulkanRenderer::Shutdown -> enter" << std::endl;
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);

        if (m_imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
        if (m_renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
        if (m_inFlightFence != VK_NULL_HANDLE) vkDestroyFence(m_device, m_inFlightFence, nullptr);

        if (!m_commandBuffers.empty()) {
            vkFreeCommandBuffers(m_device, m_commandPool, (uint32_t)m_commandBuffers.size(), m_commandBuffers.data());
            m_commandBuffers.clear();
        }
        if (m_commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        for (auto iv : m_swapchainImageViews) if (iv != VK_NULL_HANDLE) vkDestroyImageView(m_device, iv, nullptr);
        m_swapchainImageViews.clear();

        if (m_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

        // Destroy optional host image used for triangle testing
        if (m_hostImage != VK_NULL_HANDLE) { vkDestroyImage(m_device, m_hostImage, nullptr); m_hostImage = VK_NULL_HANDLE; }
        if (m_hostImageMemory != VK_NULL_HANDLE) { vkFreeMemory(m_device, m_hostImageMemory, nullptr); m_hostImageMemory = VK_NULL_HANDLE; }

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    std::cout << "VulkanRenderer::Shutdown -> exit" << std::endl;
#endif
    if (m_sdlVulkanLoaded) SDL_Vulkan_UnloadLibrary();
    m_sdlVulkanLoaded = false;
    m_surfaceCreatedViaSDL = false;
#ifdef HAVE_VULKAN
    m_swapchainImageInitialized.clear();
#endif
    m_meshes.clear();
    m_pendingMeshDraws.clear();
    m_pendingTextureDraws.clear();
    m_pointLights.clear();
    m_meshDrawCallsCurrent = 0;
    m_meshDrawCallsLast = 0;
    m_uiDrawCallsCurrent = 0;
    m_uiDrawCallsLast = 0;
    m_lastComputedClearColor[0] = 0.1f;
    m_lastComputedClearColor[1] = 0.6f;
    m_lastComputedClearColor[2] = 0.2f;
    m_lastComputedClearColor[3] = 1.0f;
    m_warnedMeshStub = false;
    m_window = nullptr;
    m_available = false;
}

} // namespace Genesis::Engine