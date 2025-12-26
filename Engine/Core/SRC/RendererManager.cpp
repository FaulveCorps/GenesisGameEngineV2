#include "ENGINE/RendererManager.h"
#include "ENGINE/GraphicsFactory.h"
#include "ENGINE/Mesh.h"
#include "ENGINE/MeshRegistry.h"
#include "ENGINE/ShaderRegistry.h"
#include "ENGINE/TextureRegistry.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace Genesis::Engine {

static std::unique_ptr<IGraphicsAPI> s_renderer;

IGraphicsAPI* RendererManager::GetRenderer() {
    return s_renderer.get();
}

void RendererManager::SetRenderer(std::unique_ptr<IGraphicsAPI> renderer) {
    s_renderer = std::move(renderer);
}

std::vector<std::string> RendererManager::CandidateRenderers() {
    // Order of preference when cycling (start with software for deterministic tests)
    std::vector<std::string> order = { "software", "wgpu", "vulkan", "d3d12", "directx" };
    const char* enableGL = std::getenv("GENESIS_ENABLE_OPENGL");
    if (enableGL && std::strcmp(enableGL, "1") == 0) {
        order.push_back("opengl");
    } else {
        std::cout << "RendererManager: skipping 'opengl' in CandidateRenderers (set GENESIS_ENABLE_OPENGL=1 to enable)" << std::endl;
    }
    return order;
}

bool RendererManager::SwitchRendererByName(const std::string& name, SDL_Window* window, SDL_GLContext ctx) {
    std::cout << "RendererManager: switching to renderer '" << name << "'" << std::endl;
    // Attempt to create and initialize the new renderer using GraphicsFactory
    auto newRenderer = GraphicsFactory::CreateRenderer(window, ctx, { name }, false);
    if (!newRenderer) {
        std::cerr << "RendererManager: failed to create renderer '" << name << "'" << std::endl;
        return false;
    }

    IGraphicsAPI* old = GetRenderer();
    // Ensure GL context is current so destroying GL resources is safe
    if (window && ctx) {
        if (SDL_GL_MakeCurrent(window, ctx) != 0) {
            std::cerr << "RendererManager: SDL_GL_MakeCurrent failed before destroying old resources: " << SDL_GetError() << std::endl;
        } else {
            std::cout << "RendererManager: SDL_GL_MakeCurrent succeeded before destroying old resources" << std::endl;
        }
    }

    // Destroy resources on the old renderer first
    MeshRegistry::Instance().DestroyAllOnRenderer(old);
    ShaderRegistry::Instance().DestroyAllOnRenderer(old);
    TextureRegistry::Instance().DestroyAllOnRenderer(old);

    if (old) {
        try {
            old->Shutdown();
        } catch (...) {
            std::cerr << "RendererManager: exception while shutting down old renderer" << std::endl;
        }
    }

    // Replace renderer
    SetRenderer(std::move(newRenderer));

    // Ensure GL context is current for resource uploads (some uploads call GL functions)
    if (window && ctx) {
        if (SDL_GL_MakeCurrent(window, ctx) != 0) {
            std::cerr << "RendererManager: SDL_GL_MakeCurrent failed after switch: " << SDL_GetError() << std::endl;
        } else {
            std::cout << "RendererManager: SDL_GL_MakeCurrent succeeded after switch" << std::endl;
        }
    }

    // Upload resources into new renderer
    MeshRegistry::Instance().UploadAllToRenderer(GetRenderer());
    ShaderRegistry::Instance().UploadAllToRenderer(GetRenderer());
    TextureRegistry::Instance().UploadAllToRenderer(GetRenderer());

    // Update the SDL window title (if available) to indicate current renderer
    if (window) {
        auto cur = GetRenderer();
        std::string title = std::string("Renderer: ") + (cur ? cur->GetName() : std::string("(none)"));
        SDL_SetWindowTitle(window, title.c_str());
    }

    std::cout << "RendererManager: switched to '" << name << "'" << std::endl;
    return true;
}

bool RendererManager::CycleRenderer(SDL_Window* window, SDL_GLContext ctx) {
    auto candidates = CandidateRenderers();
    IGraphicsAPI* cur = GetRenderer();
    std::string curName;
    // Try to guess current name by testing creation order (best-effort: compare type)
    // For simplicity, cycle using the order starting from the one after the first candidate that would construct to the same type
    // Find current index by attempting to match candidate by creating a temp renderer and comparing typeid? Instead, just advance to next candidate every call.

    static size_t lastIndex = 0;
    // Start from next candidate
    for (size_t i = 1; i <= candidates.size(); ++i) {
        size_t idx = (lastIndex + i) % candidates.size();
        if (SwitchRendererByName(candidates[idx], window, ctx)) {
            lastIndex = idx;
            return true;
        }
    }
    return false;
}

} // namespace Genesis::Engine