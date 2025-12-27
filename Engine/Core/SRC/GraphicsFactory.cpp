#include "engine/GraphicsFactory.h"
#include "engine/OpenGLRenderer.h"
#include "engine/DirectXRenderer.h"
#include "engine/VulkanRenderer.h"
#include "engine/D3D12Renderer.h"
#include "engine/WgpuRenderer.h"
#include "engine/SoftwareRenderer.h"
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace Genesis::Engine {

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
    return r;
}

std::unique_ptr<IGraphicsAPI> GraphicsFactory::CreateRenderer(SDL_Window* window, SDL_GLContext glContext, const std::vector<std::string>& priorityOrder, bool requirePresentForVulkan) {
    if (!window) return nullptr;

    // If no order specified, use sensible default: prefer wgpu, then Vulkan, DirectX, OpenGL
    std::vector<std::string> order = priorityOrder;
    if (order.empty()) {
        order = { "wgpu", "vulkan", "d3d12", "directx", "opengl" };
        const char* enableGL = std::getenv("GENESIS_ENABLE_OPENGL");
        if (!(enableGL && std::strcmp(enableGL, "1") == 0)) {
            order.erase(std::remove(order.begin(), order.end(), "opengl"), order.end());
            std::cout << "GraphicsFactory: OpenGL disabled via GENESIS_ENABLE_OPENGL; skipping opengl in default order" << std::endl;
        }
    }

    for (const auto& entry : order) {
        std::string name = toLower(entry);
        std::cout << "GraphicsFactory: attempting renderer '" << entry << "'" << std::endl;

        if (name == "vulkan") {
#ifdef HAVE_VULKAN
            auto r = std::make_unique<VulkanRenderer>();
            if (r->Init(window, glContext)) {
                // If the caller requested strict Vulkan (requires present), verify capability
                if (requirePresentForVulkan) {
                    // Query whether Vulkan renderer has swapchain/present capability
                    // VulkanRenderer::IsPresentCapable is a lightweight check
                    auto vr = static_cast<VulkanRenderer*>(r.get());
                    if (!vr->IsPresentCapable()) {
                        std::cerr << "GraphicsFactory: Vulkan initialized but not present-capable; trying next" << std::endl;
                        r->Shutdown();
                    } else {
                        std::cout << "GraphicsFactory: selected VulkanRenderer (present-capable)" << std::endl;
                        return r;
                    }
                } else {
                    std::cout << "GraphicsFactory: selected VulkanRenderer" << std::endl;
                    return r;
                }
            }
            std::cerr << "GraphicsFactory: VulkanRenderer::Init failed; trying next" << std::endl;
            r->Shutdown();
#else
            std::cout << "GraphicsFactory: skipping VulkanRenderer (HAVE_VULKAN not defined)" << std::endl;
#endif
        } else if (name == "wgpu") {
#ifdef HAVE_WGPU
            const char* _env_enable_wgpu = std::getenv("GENESIS_ENABLE_WGPU");
            if (!_env_enable_wgpu || std::strcmp(_env_enable_wgpu, "1") != 0) {
                std::cout << "GraphicsFactory: skipping WgpuRenderer (disabled via GENESIS_ENABLE_WGPU)" << std::endl;
            } else {
                auto r = std::make_unique<WgpuRenderer>();
                if (r->Init(window, glContext)) {
                    std::cout << "GraphicsFactory: selected WgpuRenderer" << std::endl;
                    return r;
                }
                std::cerr << "GraphicsFactory: WgpuRenderer::Init failed; trying next" << std::endl;
                r->Shutdown();
            }
#else
            std::cout << "GraphicsFactory: skipping WgpuRenderer (HAVE_WGPU not defined)" << std::endl;
#endif
        } else if (name == "d3d12" || name == "directx12") {
#ifdef _WIN32
            auto r = std::make_unique<D3D12Renderer>();
            if (r->Init(window, glContext)) {
                std::cout << "GraphicsFactory: selected D3D12Renderer" << std::endl;
                return r;
            }
            std::cerr << "GraphicsFactory: D3D12Renderer::Init failed; trying next" << std::endl;
            r->Shutdown();
#else
            std::cout << "GraphicsFactory: skipping D3D12Renderer (not _WIN32)" << std::endl;
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
            std::cout << "GraphicsFactory: creating OpenGLRenderer and calling Init" << std::endl;
            const char* _env_enable_gl = std::getenv("GENESIS_ENABLE_OPENGL");
            if (!(_env_enable_gl && std::strcmp(_env_enable_gl, "1") == 0)) {
                std::cout << "GraphicsFactory: explicit OpenGL request skipped (GENESIS_ENABLE_OPENGL not set)" << std::endl;
                continue;
            }
            auto r = std::make_unique<OpenGLRenderer>();
            std::cout << "GraphicsFactory: OpenGLRenderer::Init -> calling" << std::endl;
            bool initRes = r->Init(window, glContext);
            std::cout << "GraphicsFactory: OpenGLRenderer::Init -> returned res=" << initRes << std::endl;
            if (initRes) {
                std::cout << "GraphicsFactory: selected OpenGLRenderer" << std::endl;
                return r;
            }
            std::cerr << "GraphicsFactory: OpenGLRenderer::Init failed; trying next" << std::endl;
            r->Shutdown();
        } else if (name == "software" || name == "null") {
            auto r = std::make_unique<SoftwareRenderer>();
            if (r->Init(window, glContext)) {
                std::cout << "GraphicsFactory: selected SoftwareRenderer" << std::endl;
                return r;
            }
            std::cerr << "GraphicsFactory: SoftwareRenderer::Init failed; trying next" << std::endl;
            r->Shutdown();
        } else {
            std::cout << "GraphicsFactory: unknown renderer name '" << entry << "'" << std::endl;
        }
    }

    std::cerr << "GraphicsFactory: no suitable renderer found" << std::endl;
    return nullptr;
}

} // namespace Genesis::Engine
