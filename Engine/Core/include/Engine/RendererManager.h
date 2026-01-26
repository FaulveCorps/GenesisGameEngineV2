#pragma once

#include <memory>
#include <string>
#include <vector>
#include <SDL.h>
#include "engine/IGraphics.h"

namespace Genesis::Engine {

class RendererManager {
public:
    // Get the current renderer (may be nullptr)
    static IGraphicsAPI* GetRenderer();

    // Replace the current renderer with an already-initialized renderer
    static void SetRenderer(std::unique_ptr<IGraphicsAPI> renderer);

    // Shutdown the current renderer and clear it from the manager
    static void ShutdownRenderer();

    // Try to switch to a renderer by name (uses GraphicsFactory internally) - returns true if switched
    static bool SwitchRendererByName(const std::string& name, SDL_Window* window, SDL_GLContext ctx);

    // Cycle to the next available renderer in a fixed candidate list
    static bool CycleRenderer(SDL_Window* window, SDL_GLContext ctx);

    // Return default candidate list
    static std::vector<std::string> CandidateRenderers();
};

} // namespace Genesis::Engine