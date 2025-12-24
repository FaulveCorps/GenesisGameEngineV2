#include "engine/GraphicsFactory.h"
#include "engine/OpenGLRenderer.h"
#include "engine/DirectXRenderer.h"
#include "engine/VulkanRenderer.h"
#include <algorithm>
#include <iostream>

namespace Genesis::Engine {

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
    return r;
}

std::unique_ptr<IGraphicsAPI> GraphicsFactory::CreateRenderer(SDL_Window* window, SDL_GLContext glContext, const std::vector<std::string>& priorityOrder) {
    if (!window) return nullptr;

    // If no order specified, use sensible default: Vulkan, DirectX, OpenGL
    std::vector<std::string> order = priorityOrder;
    if (order.empty()) order = { "vulkan", "directx", "opengl" };

    for (const auto& entry : order) {
        std::string name = toLower(entry);
        std::cout << "GraphicsFactory: attempting renderer '" << entry << "'" << std::endl;

        if (name == "vulkan") {
#ifdef HAVE_VULKAN
            auto r = std::make_unique<VulkanRenderer>();
            if (r->Init(window, glContext)) {
                std::cout << "GraphicsFactory: selected VulkanRenderer" << std::endl;
                return r;
            }
            std::cerr << "GraphicsFactory: VulkanRenderer::Init failed; trying next" << std::endl;
            r->Shutdown();
#else
            std::cout << "GraphicsFactory: skipping VulkanRenderer (HAVE_VULKAN not defined)" << std::endl;
#endif
        } else if (name == "directx" || name == "d3d11") {
#ifdef _WIN32
            auto r = std::make_unique<DirectXRenderer>();
            if (r->Init(window, glContext)) {
                std::cout << "GraphicsFactory: selected DirectXRenderer" << std::endl;
                return r;
            }
            std::cerr << "GraphicsFactory: DirectXRenderer::Init failed; trying next" << std::endl;
            r->Shutdown();
#else
            std::cout << "GraphicsFactory: skipping DirectXRenderer (not _WIN32)" << std::endl;
#endif
        } else if (name == "opengl" || name == "gl") {
            auto r = std::make_unique<OpenGLRenderer>();
            if (r->Init(window, glContext)) {
                std::cout << "GraphicsFactory: selected OpenGLRenderer" << std::endl;
                return r;
            }
            std::cerr << "GraphicsFactory: OpenGLRenderer::Init failed; trying next" << std::endl;
            r->Shutdown();
        } else {
            std::cout << "GraphicsFactory: unknown renderer name '" << entry << "'" << std::endl;
        }
    }

    std::cerr << "GraphicsFactory: no suitable renderer found" << std::endl;
    return nullptr;
}

} // namespace Genesis::Engine
