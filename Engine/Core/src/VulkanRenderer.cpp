#include "engine/VulkanRenderer.h"
#include <SDL_vulkan.h>
#include <SDL_syswm.h>
#include <iostream>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#endif
namespace Genesis::Engine {

bool VulkanRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
    m_window = window;

    // Try to load Vulkan loader via SDL (optional); fall back to platform Win32 surface if SDL does not expose Vulkan
    bool sdlVulkan = (SDL_Vulkan_LoadLibrary(nullptr) == 0);
    m_sdlVulkan = sdlVulkan;
    std::vector<const char*> extensions;
    if (sdlVulkan) {
        unsigned int count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr)) {
            std::cerr << "VulkanRenderer: SDL_Vulkan_GetInstanceExtensions failed" << std::endl;
            if (sdlVulkan) SDL_Vulkan_UnloadLibrary();
            return false;
        }
        extensions.resize(count);
        if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, extensions.data())) {
            std::cerr << "VulkanRenderer: SDL_Vulkan_GetInstanceExtensions failed (2)" << std::endl;
            if (sdlVulkan) SDL_Vulkan_UnloadLibrary();
            return false;
        }
        std::cout << "VulkanRenderer: SDL reported " << count << " required instance extensions:\n";
        for (unsigned int i = 0; i < count; ++i) std::cout << "  " << extensions[i] << std::endl;
    } else {
#ifdef _WIN32
        std::cout << "VulkanRenderer: SDL reports no dynamic Vulkan support; falling back to Win32 surface creation" << std::endl;
        extensions.push_back("VK_KHR_surface");
        extensions.push_back("VK_KHR_win32_surface");
#else
        std::cerr << "VulkanRenderer: SDL Vulkan not available and no fallback for this platform" << std::endl;
        return false;
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
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::cout << "VulkanRenderer: instance created" << std::endl;

    // Create a surface via SDL if available; otherwise try a platform-specific surface (Win32)
    if (sdlVulkan) {
        if (!SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface)) {
            std::cerr << "VulkanRenderer: SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << std::endl;
            m_available = false;
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            if (sdlVulkan) SDL_Vulkan_UnloadLibrary();
            return false;
        }
    } else {
#ifdef _WIN32
        SDL_SysWMinfo wminfo;
        SDL_VERSION(&wminfo.version);
        if (!SDL_GetWindowWMInfo(m_window, &wminfo)) {
            std::cerr << "VulkanRenderer: SDL_GetWindowWMInfo failed" << std::endl;
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            return false;
        }
        VkWin32SurfaceCreateInfoKHR sc{};
        sc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sc.hwnd = wminfo.info.win.window;
        sc.hinstance = wminfo.info.win.hinstance;
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
        if (m_sdlVulkan) SDL_Vulkan_UnloadLibrary();
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
        if (m_sdlVulkan) SDL_Vulkan_UnloadLibrary();
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
        if (m_sdlVulkan) SDL_Vulkan_UnloadLibrary();
        return false;
    }

    std::cout << "VulkanRenderer: logical device created" << std::endl;
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    if (m_presentQueueFamily != m_graphicsQueueFamily) vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    else m_presentQueue = m_graphicsQueue;
    std::cout << "VulkanRenderer: obtained graphics and present queues" << std::endl;
    // If we fell back to Win32 surface creation (SDL didn't expose Vulkan), do not attempt a full swapchain here — this environment sometimes crashes with certain drivers.
    if (!m_sdlVulkan) {
        std::cout << "VulkanRenderer: Win32 fallback - skipping swapchain creation to avoid driver issues. Device initialized for smoke test." << std::endl;
        m_available = true;
        return true;
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
        SDL_Vulkan_GetDrawableSize(m_window, &w, &h);
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

    // Semaphores and fence
    VkSemaphoreCreateInfo sci_sem{};
    sci_sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(m_device, &sci_sem, nullptr, &m_imageAvailableSemaphore);
    vkCreateSemaphore(m_device, &sci_sem, nullptr, &m_renderFinishedSemaphore);

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(m_device, &fci, nullptr, &m_inFlightFence);

    // Record simple command buffers that clear each swapchain image to a color
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(m_commandBuffers[i], &cbbi);

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
    // no per-frame CPU-side work required for this simple smoke test
    if (!m_available) return;
}

void VulkanRenderer::EndFrame() {
    if (!m_available) return;
#ifdef HAVE_VULKAN
    if (m_device == VK_NULL_HANDLE) return;
    if (m_swapchain == VK_NULL_HANDLE) return; // no swapchain (skipped for this environment)

    // Wait for previous frame to finish
    vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlightFence);

    uint32_t imageIndex = 0;
    VkResult r = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (r != VK_SUCCESS) {
        std::cerr << "VulkanRenderer: vkAcquireNextImageKHR failed: " << r << std::endl;
        return;
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
#endif
}

void VulkanRenderer::Shutdown() {
#ifdef HAVE_VULKAN
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
#endif
    SDL_Vulkan_UnloadLibrary();
    m_window = nullptr;
    m_available = false;
}

} // namespace Genesis::Engine