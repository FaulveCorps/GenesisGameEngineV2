#include "engine/RendererManager.h"
#include "engine/GraphicsFactory.h"
#include "engine/Mesh.h"
#include "engine/MeshRegistry.h"
#include <iostream>

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
    return { "software", "wgpu", "vulkan", "d3d12", "directx", "opengl" };
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
    // Destroy resources on the old renderer first
    MeshRegistry::Instance().DestroyAllOnRenderer(old);

    if (old) {
        try {
            old->Shutdown();
        } catch (...) {
            std::cerr << "RendererManager: exception while shutting down old renderer" << std::endl;
        }
    }

    // Replace renderer
    SetRenderer(std::move(newRenderer));

    // Upload resources into new renderer
    MeshRegistry::Instance().UploadAllToRenderer(GetRenderer());

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